#!/usr/bin/env python3
"""ff_screenshot.py - screenshot a web page in headless Firefox after it has finished drawing.

usage: ff_screenshot.py URL OUT.png [--size 1400x820] [--wait-for CSS] [--timeout 30]
                        [--eval JS]... [--sleep SECONDS] [--element CSS]

  --size      browser window size (default 1400x820)
  --wait-for  wait until an element matching this CSS selector exists (e.g. the desktop icons,
              which the otsdaq desktop adds after the page's load event)
  --frame     run --eval in this frame: 0 = top page (default: the frame where --wait-for matched,
              else the deepest frame; otsdaq pages such as the desktop run inside a frame)
  --eval      JavaScript to run after loading, in order, each followed by --sleep (repeatable),
              e.g. to click an icon:  --eval "document.querySelector('#icon-Configure').click()"
  --sleep     seconds to wait after loading and after each --eval (default 1)
  --element   capture only the element matching this CSS selector instead of the window

`firefox --screenshot` captures at the load event, before pages like the otsdaq desktop have fetched
their content. This drives Firefox through its built-in WebDriver BiDi protocol instead
(--remote-debugging-port, Firefox >= 129), with a private temporary profile, using only the Python
standard library: no Selenium, geckodriver or pip packages. Read-only toward the page except for what
--eval does.
"""
import argparse, base64, json, os, shutil, socket, struct, subprocess, sys, tempfile, time


class WS:
    """minimal WebSocket client (RFC 6455): text frames, client masking, enough for BiDi"""

    def __init__(self, host, port, path):
        self.s = socket.create_connection((host, port), timeout=60)
        key = base64.b64encode(os.urandom(16)).decode()
        self.s.sendall(('GET %s HTTP/1.1\r\nHost: %s:%d\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n'
                        'Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n' % (path, host, port, key)).encode())
        resp = b''
        while b'\r\n\r\n' not in resp:
            resp += self.s.recv(4096)
        if b' 101 ' not in resp.split(b'\r\n')[0]:
            raise RuntimeError('WebSocket upgrade failed: %r' % resp[:200])
        self.buf = resp.split(b'\r\n\r\n', 1)[1]

    def _recv(self, n):
        while len(self.buf) < n:
            chunk = self.s.recv(1 << 20)
            if not chunk:
                raise RuntimeError('WebSocket closed')
            self.buf += chunk
        out, self.buf = self.buf[:n], self.buf[n:]
        return out

    def send(self, text):
        data = text.encode()
        head = bytes([0x81])
        n = len(data)
        head += bytes([0x80 | n]) if n < 126 else (bytes([0x80 | 126]) + struct.pack('>H', n) if n < 65536 else bytes([0x80 | 127]) + struct.pack('>Q', n))
        mask = os.urandom(4)
        self.s.sendall(head + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(data)))

    def recv(self):
        msg = b''
        while True:
            b0, b1 = self._recv(2)
            n = b1 & 0x7f
            if n == 126:
                n = struct.unpack('>H', self._recv(2))[0]
            elif n == 127:
                n = struct.unpack('>Q', self._recv(8))[0]
            payload = self._recv(n)
            op = b0 & 0x0f
            if op == 8:
                raise RuntimeError('WebSocket closed by browser')
            if op in (0, 1, 2):
                msg += payload
                if b0 & 0x80:
                    return msg.decode()


class Bidi:
    def __init__(self, ws):
        self.ws, self.next_id = ws, 0

    def cmd(self, method, params):
        self.next_id += 1
        self.ws.send(json.dumps({'id': self.next_id, 'method': method, 'params': params}))
        while True:  # skip events until our reply arrives
            m = json.loads(self.ws.recv())
            if m.get('id') == self.next_id:
                if m.get('type') == 'error':
                    raise RuntimeError('%s: %s %s' % (method, m.get('error'), m.get('message')))
                return m['result']


def free_port():
    s = socket.socket()
    s.bind(('127.0.0.1', 0))
    p = s.getsockname()[1]
    s.close()
    return p


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('url'); ap.add_argument('out')
    ap.add_argument('--size', default='1400x820'); ap.add_argument('--wait-for')
    ap.add_argument('--timeout', type=float, default=30); ap.add_argument('--eval', action='append', default=[])
    ap.add_argument('--sleep', type=float, default=1.0); ap.add_argument('--element')
    ap.add_argument('--frame', type=int)
    a = ap.parse_args()
    w, h = (int(x) for x in a.size.lower().split('x'))
    port, prof = free_port(), tempfile.mkdtemp(prefix='ffshot.')
    ff = subprocess.Popen(['firefox', '--headless', '--no-remote', '--profile', prof, '--remote-debugging-port', str(port)],
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        for _ in range(100):  # wait for the BiDi endpoint
            try:
                ws = WS('127.0.0.1', port, '/session'); break
            except OSError:
                time.sleep(0.2)
        else:
            sys.exit('ERROR: Firefox did not open its remote debugging port')
        b = Bidi(ws)
        b.cmd('session.new', {'capabilities': {}})
        ctx = b.cmd('browsingContext.getTree', {})['contexts'][0]['context']
        b.cmd('browsingContext.setViewport', {'context': ctx, 'viewport': {'width': w, 'height': h}})
        b.cmd('browsingContext.navigate', {'context': ctx, 'url': a.url, 'wait': 'complete'})

        def frames():  # top page first, then nested frames depth-first
            out = []
            def walk(c):
                out.append(c['context'])
                for ch in c.get('children') or []:
                    walk(ch)
            walk(b.cmd('browsingContext.getTree', {'root': ctx})['contexts'][0])
            return out

        def js(expr, context):
            r = b.cmd('script.evaluate', {'expression': expr, 'target': {'context': context}, 'awaitPromise': True})
            if r.get('type') == 'exception':
                raise RuntimeError('JavaScript error: %s' % r['exceptionDetails'].get('text'))
            return r['result'].get('value')
        target = None
        if a.wait_for:
            t0 = time.time()
            while target is None:
                for c in frames():
                    try:
                        if js('!!document.querySelector(%s)' % json.dumps(a.wait_for), c):
                            target = c; break
                    except RuntimeError:
                        pass  # frame still loading
                else:
                    if time.time() - t0 > a.timeout:
                        sys.exit('ERROR: %s did not appear within %gs in any frame' % (a.wait_for, a.timeout))
                    time.sleep(0.25)
        time.sleep(a.sleep)
        fl = frames()
        if a.frame is not None:
            target = fl[a.frame]
        elif target is None:
            target = fl[-1]
        for e in a.eval:
            print('eval:', js(e, target))
            time.sleep(a.sleep)
        params = {'context': ctx}
        if a.element:
            node = b.cmd('browsingContext.locateNodes', {'context': ctx, 'locator': {'type': 'css', 'value': a.element}})['nodes']
            if not node:
                sys.exit('ERROR: no element matches %s' % a.element)
            params['clip'] = {'type': 'element', 'element': {'sharedId': node[0]['sharedId']}}
        png = base64.b64decode(b.cmd('browsingContext.captureScreenshot', params)['data'])
        with open(a.out, 'wb') as f:
            f.write(png)
        print('%s  %dx%d viewport, %d bytes' % (a.out, w, h, len(png)))
        try:
            b.cmd('session.end', {})
        except Exception:
            pass
    finally:
        ff.terminate()
        try:
            ff.wait(10)
        except subprocess.TimeoutExpired:
            ff.kill()
        shutil.rmtree(prof, ignore_errors=True)


if __name__ == '__main__':
    main()

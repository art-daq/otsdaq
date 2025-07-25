#!/bin/env python3

"""
OTSDAQ Command-line utilities

send_transition
get_status

"""

import click
import enum
import xml.etree.ElementTree as ET
from send_gateway_command import send_gateway_command

def host_port_from_ctx(ctx):
    ctx.ensure_object(dict)
    return [ctx.obj['HOST'], ctx.obj['PORT']]

@click.group()
@click.option("--host", default="localhost")
@click.option("--port", default=2020)
@click.pass_context
def cli(ctx, host, port):
    ctx.obj['HOST'] = host
    ctx.obj['PORT'] = port
    pass

@cli.command()
@click.argument("state_machine")
# Transition names from otsdaq/FiniteStateMachine/RunControlStateMachine.cc
@click.argument("cmd", type=click.Choice(["Shutdown","Startup","Initialize","Fail","Configure","Halt","Abort","Pause","Resume","Start","Stop"], case_sensitive=False))
@click.argument("parameters", nargs=-1)
@click.pass_context
def send_transition(ctx, state_machine, cmd, parameters):
    host,port = host_port_from_ctx(ctx)
    result = send_gateway_command(host, port, state_machine, [cmd] + list(parameters))
    if result == "Done":
        print(f"Command {cmd} completed successfully")
    else:
        print(f"Command had error:\n{result}")
    pass

@cli.command()
@click.pass_context
def get_state_machines(ctx):
    host,port = host_port_from_ctx(ctx)
    result = send_gateway_command(host,port, "GetStateMachineNames", [])
    root = ET.fromstring(result)
    active = root.find('./DATA/active')
    activefsm = ""
    if active is not None:
        activefsm = active.get('value')
        print(f"Active FSM is: {activefsm}")

    print("All FSMs:")
    for fsm in root.iter('fsm'):
        thisfsm = fsm.get('value')
        print("  " + fsm.get('value'))
    pass

if __name__ == "__main__": 
    cli(obj={})

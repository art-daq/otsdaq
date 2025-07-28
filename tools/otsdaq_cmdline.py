#!/bin/env python3

"""
OTSDAQ Command-line utilities

send_transition
get_status

"""

from rich.console import Console
from rich.table import Table
from rich.progress_bar import ProgressBar
from rich import print
import rich_click as click
import enum
from urllib.parse import unquote
from datetime import datetime
import xml.etree.ElementTree as ET
from send_gateway_command import send_gateway_command


def host_port_from_ctx(ctx):
    ctx.ensure_object(dict)
    return [ctx.obj["HOST"], ctx.obj["PORT"]]


def state_machines(host, port):
    ret = {}
    result = send_gateway_command(host, port, "GetStateMachineNames", [])
    root = ET.fromstring(result)
    active = root.find("./DATA/active")
    activefsm = ""
    if active is not None:
        activefsm = unquote(active.get("value"))
        ret["active"] = activefsm

    ret["fsm"] = []
    for fsm in root.iter("fsm"):
        thisfsm = unquote(fsm.get("value"))
        ret["fsm"] += [thisfsm]
    return ret


@click.group()
@click.option("--host", default="localhost")
@click.option("--port", default=2020)
@click.pass_context
def cli(ctx, host, port):
    ctx.obj["HOST"] = host
    ctx.obj["PORT"] = port


@cli.command()
@click.argument("state_machine")
# Transition names from otsdaq/FiniteStateMachine/RunControlStateMachine.cc
@click.argument(
    "cmd",
    type=click.Choice(
        [
            "Shutdown",
            "Startup",
            "Initialize",
            "Fail",
            "Configure",
            "Halt",
            "Abort",
            "Pause",
            "Resume",
            "Start",
            "Stop",
        ],
        case_sensitive=False,
    ),
)
@click.argument("parameters", nargs=-1)
@click.pass_context
def send_transition(ctx, state_machine, cmd, parameters):
    host, port = host_port_from_ctx(ctx)

    fsms = state_machines(host, port)
    if "active" in fsms is not None and state_machine != fsms["active"]:
        print(
            f"Cannot send {cmd} to {state_machine} because FSM {fsms['active']} is currently active"
        )
        return

    if state_machine not in fsms["fsm"]:
        print(
            f"Cannot send {cmd} to {state_machine} because it is not in the list of FSMs for this ots instance"
        )
        return

    result = send_gateway_command(host, port, state_machine, [cmd] + list(parameters))
    if result == "Done" or result == "":
        print(f"Command {cmd} completed successfully")
    else:
        print(f"Command had error:\n{result}")


@cli.command()
@click.pass_context
def get_state_machines(ctx):
    host, port = host_port_from_ctx(ctx)
    result = state_machines(host, port)

    if "active" in result and result["active"] is not None:
        print(f"Active FSM is: {result['active']}")

    print("All FSMs:")
    for fsm in result["fsm"]:
        print("  " + fsm)


@cli.command()
@click.pass_context
@click.option("--detail", default=False, is_flag=True)
def get_app_status(ctx, detail):
    host, port = host_port_from_ctx(ctx)
    result = send_gateway_command(host, port, "GetRemoteGatewayStatusXML", [])
    console = Console()

    try:
        root = ET.fromstring(result)
        systemmessages = root.find("./systemMessages")
        if systemmessages is not None:
            # Format is: targetUser | time | msg | targetUser | time | msg...etc
            messagestr = unquote(systemmessages.get("value"))
            messagearr = messagestr.split("|")
            if len(messagearr) > 1:
                table = Table(title="System Messages")
                table.add_column("User")
                table.add_column("Time")
                table.add_column("Message")

                user, time, msg, *rest = messagearr
                ts = str(datetime.fromtimestamp(int(time)))
                table.add_row(user, ts, msg)
                while len(rest) > 0:
                    user, time, msg, *rest = rest
                    ts = str(datetime.fromtimestamp(int(time)))
                    table.add_row(user, ts, msg)
                console.print(table)

        userlock = root.find("./usernameWithLock")
        if userlock is not None:
            print(f"User [bold]{userlock.get('value')}[/bold] has the lock")

        consolewarn = root.find("./console_warn_count")
        consoleerr = root.find("./console_err_count")
        if consolewarn is not None and consoleerr is not None:
            print(
                f"There have been {consoleerr.get('value')} error(s) and {consolewarn.get('value')} warning(s) reported to the ots console"
            )

        stable = Table(title="Supervisor Status")
        stable.add_column("Context Name")
        stable.add_column("App Name")
        stable.add_column("Status")
        stable.add_column("Progress")
        stable.add_column("Detail")
        stable.add_column("App Type")
        stable.add_column("App URL")
        stable.add_column("App ID")
        for supervisor in root.iter("supervisor"):
            context = unquote(supervisor.get("context"))
            name = unquote(supervisor.get("name"))
            status = unquote(supervisor.get("status"))
            progressPercentage = int(supervisor.get("progress"))
            progress = ProgressBar(completed=progressPercentage,width=10)
            detail = unquote(supervisor.find("detail").get("value"))
            apptype = unquote(supervisor.get("class"))
            appurl = unquote(supervisor.get("url"))
            appid = unquote(supervisor.get("id"))
            stable.add_row(context, name, status, progress, detail, apptype, appurl, appid)

        console.print(stable)
    except ET.ParseError as ex:
        print(f"Exception from XML Parser: {ex}. Printing XML document:")
        print(result)


if __name__ == "__main__":
    cli(obj={})

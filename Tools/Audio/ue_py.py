r"""Run a Python file inside the RUNNING Unreal editor, from outside it.

Unreal's Python plugin can listen for commands over the local network
("Remote Execution", Project Settings > Plugins > Python). That is how an
outside tool reaches into a live editor -- which is what lets this project's
audio assets be built by script instead of by clicking through menus.

    python ue_py.py <script.py>

Requires: the editor open, and bRemoteExecution = true.
"""
import os
import sys
import time

ENGINE_PYTHON = os.path.join(
    "C:", os.sep, "Program Files", "Epic Games", "UE_5.8", "Engine", "Plugins",
    "Experimental", "PythonScriptPlugin", "Content", "Python")
sys.path.append(ENGINE_PYTHON)

import remote_execution  # noqa: E402  (path must be set first)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    script = os.path.abspath(sys.argv[1])

    conn = remote_execution.RemoteExecution()
    conn.start()
    deadline = time.time() + 15
    while not conn.remote_nodes and time.time() < deadline:
        time.sleep(0.4)

    if not conn.remote_nodes:
        print("NO EDITOR FOUND. Is the editor open with Remote Execution enabled?")
        conn.stop()
        return 1

    node = conn.remote_nodes[0]
    print("editor node: %s" % node.get("node_id", "?"))
    conn.open_command_connection(node)
    result = conn.run_command(script, unattended=True,
                              exec_mode=remote_execution.MODE_EXEC_FILE,
                              raise_on_failure=False)
    conn.stop()

    for entry in result.get("output") or []:
        print("[%s] %s" % (entry.get("type"), entry.get("output")))
    print("success: %s" % result.get("success"))
    if result.get("result"):
        print("result: %s" % result.get("result"))
    return 0 if result.get("success") else 1


if __name__ == "__main__":
    raise SystemExit(main())

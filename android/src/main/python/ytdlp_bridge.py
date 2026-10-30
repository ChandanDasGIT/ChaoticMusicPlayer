# Runs yt-dlp inside the app, the way the desktop runs the yt-dlp program: the
# same arguments, giving back its exit code and what it printed
# (backend/system/process/process_android.cpp calls this through YtDlpRunner).
import io
import sys
import threading
from contextlib import redirect_stderr, redirect_stdout

_lock = threading.Lock()  # one at a time: printing goes through sys.stdout
_source = None            # "yt-dlp" (the copy in the app) or a newer one's path


def _use(source):
    """Imports yt-dlp from the copy in the app, or from a newer one the app downloaded."""
    global _source
    if source == _source:
        return
    for name in list(sys.modules):
        if name == "yt_dlp" or name.startswith("yt_dlp."):
            del sys.modules[name]
    if _source and _source != "yt-dlp" and _source in sys.path:
        sys.path.remove(_source)
    if source != "yt-dlp":
        sys.path.insert(0, source)  # yt-dlp's release file is a zip Python imports from
    _source = source


def run(args):
    """args[0]: "yt-dlp" or a downloaded copy's path; the rest: yt-dlp's arguments.
    Returns [exit code, output, errors]."""
    args = list(args)
    with _lock:
        out, err = io.StringIO(), io.StringIO()
        code = 0
        with redirect_stdout(out), redirect_stderr(err):
            try:
                _use(args[0])
                import yt_dlp
                yt_dlp.main(args[1:])
            except SystemExit as exit:
                code = exit.code if isinstance(exit.code, int) else (0 if exit.code is None else 1)
            except Exception as error:  # noqa: BLE001 (any failure is reported like yt-dlp's own)
                err.write(f"ERROR: {error}\n")
                code = 1
        return [code, out.getvalue(), err.getvalue()]

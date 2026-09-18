"""Run the generated browser module against independently produced C++ bytes."""

import argparse
import functools
import http.server
import pathlib
import subprocess
import tempfile
import threading


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    """Serve only the selected build directory without per-request logging."""

    extensions_map = {
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        ".mjs": "text/javascript",
        ".html": "text/html; charset=utf-8",
    }

    def log_message(self, format, *args):
        """Keep CTest output limited to the verification result."""


def main():
    """Launch a private headless browser session and require the page's success marker."""
    parser = argparse.ArgumentParser()
    parser.add_argument("--browser", required=True)
    parser.add_argument("--directory", required=True)
    args = parser.parse_args()
    directory = pathlib.Path(args.directory).resolve(strict=True)
    handler = functools.partial(QuietHandler, directory=str(directory))
    with http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler) as server:
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            with tempfile.TemporaryDirectory(prefix="browser-profile-", dir=directory) as profile:
                # Cleanup is restricted to the private directory created under this build root.
                assert pathlib.Path(profile).resolve().is_relative_to(directory)
                url = f"http://127.0.0.1:{server.server_port}/generated/javascript/browser.html"
                result = subprocess.run(
                    [args.browser, "--headless=new", "--disable-gpu", "--no-first-run",
                     "--no-default-browser-check", "--disable-background-networking",
                     f"--user-data-dir={profile}", "--virtual-time-budget=10000", "--dump-dom", url],
                    capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=45,
                )
                (directory / "browser-result.html").write_text(result.stdout, encoding="utf-8")
                if result.returncode or 'data-result="pass"' not in result.stdout:
                    raise RuntimeError(f"Browser round trip failed:\n{result.stdout}\n{result.stderr}")
        finally:
            server.shutdown()
            thread.join(timeout=5)
    print("Browser round trips and C++ -> browser interoperability passed")


if __name__ == "__main__":
    main()

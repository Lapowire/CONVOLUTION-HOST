"""Bounded GitHub annotations for native failures; never include environment data."""


def error(title, output):
    lines = output.splitlines()
    failures = [
        line
        for line in lines
        if any(
            word in line
            for word in (
                "FAIL",
                "Error",
                "error:",
                "FAILED",
                "AssertionError",
                "Traceback",
            )
        )
    ]
    excerpt = "\n".join(failures[:20]) + "\n" + "\n".join(lines[-35:])
    excerpt = excerpt[-6000:]
    escape = (
        lambda value: value.replace("%", "%25")
        .replace("\r", "%0D")
        .replace("\n", "%0A")
    )
    print(f"::error title={escape(title)}::{escape(excerpt)}", flush=True)

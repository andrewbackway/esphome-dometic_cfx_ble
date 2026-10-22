"""Create ci.yaml from example.yaml using the local component instead of GitHub."""

import re
from pathlib import Path

root = Path(__file__).resolve().parent.parent
text = (root / "example.yaml").read_text(encoding="utf-8")
text, count = re.subn(
    r"external_components:\n\s+source: github://.*",
    "external_components:\n  - source:\n      type: local\n      path: components",
    text,
)
if count != 1:
    raise SystemExit("external_components block not found in example.yaml")
(root / "ci.yaml").write_text(text, encoding="utf-8")
(root / "secrets.yaml").write_text(
    'wifi_ssid: "ci"\nwifi_password: "ci-password"\n', encoding="utf-8"
)

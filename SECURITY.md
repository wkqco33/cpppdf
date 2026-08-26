# Security Policy

## Supported Versions

Security fixes are applied to the latest version on the `master` branch. Older releases may not receive fixes.

## Reporting a Vulnerability

Please do not disclose exploitable parser bugs in a public issue. Report them privately through the repository maintainer's GitHub security advisory channel, or contact the maintainer listed on the project homepage.

Include the affected version, a minimal synthetic PDF or reproduction, expected and actual behavior, and any relevant stack trace. Do not include personal documents or secrets.

This library parses untrusted PDF input. Applications should use resource limits and isolate parsing when processing files from untrusted sources.

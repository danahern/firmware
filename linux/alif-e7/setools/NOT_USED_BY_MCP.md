# This directory is NOT used by the alif-flash MCP server

The MCP reads from `tools/setools/build/` at the workspace root.

Stage all artifacts (DTBs, kernels, TF-A binaries) to:
```
<workspace>/tools/setools/build/images/
```

Flash configs go to:
```
<workspace>/tools/setools/build/config/
```

This directory is the original Alif SE Tools unpack location and is kept
for reference only. See `retrospective/setools-path-confusion.md` for
the full story of why this matters.

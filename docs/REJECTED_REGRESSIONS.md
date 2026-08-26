# Rejected regressions from development

These branches are useful because they define what the clean rewrite must avoid.

| Branch | Change | Result | Rule |
|---|---|---|---|
| v0.25b/c/d | renderer/position experiments | FPS moved from top-left to the right until disappearing | reset anchor on every update |
| v0.27a | removed Scene wait | FPS overlay disappeared | preserve Scene readiness |
| v0.27b/c | split loader with middle detach/re-attach | short freeze, no FPS overlay | do not break loader state |
| v0.28a | removed `elfldr_payload_args()` | almost instant Run, KP when game launched | full stable init is mandatory |
| v0.28b | async parent return + complete stable child init | fast and stable | this is the public v1.0.0 model |

Do not copy an experimental optimization into source-built code merely because
it looked faster in isolation.

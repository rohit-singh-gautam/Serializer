# Using the repository agent skill

[Back to the project overview](../README.md)

For the shortest path, give your coding agent the
[copyable integration request](../README.md#integrate-with-a-coding-agent).
This guide explains discovery, packaging, and use from another project.

## Repository skill

The [serializer-integration skill](../.agents/skills/serializer-integration/SKILL.md)
guides an agent through using this library in a C++ application: schema design,
`stable_ids` adoption, generated headers, protocol selection, and bounded input.

### Why the directory starts with a dot

A leading `.` is a hidden-directory convention, not a Git ignore rule. This skill
is tracked in Git and included with the source repository. The repository's
`.gitignore` excludes `.vscode/*` but does not exclude `.agents/`. Keep `.agents`
when copying or packaging the checkout; some file browsers and shell wildcards
omit hidden entries.

It follows the open [Agent Skills format](https://agentskills.io/specification):
a folder with a `SKILL.md` containing YAML `name` and `description`, followed by
Markdown instructions. Repository discovery locations are host-specific. This
repository uses Codex's `.agents/skills` convention:

```text
.agents/skills/serializer-integration/
  SKILL.md
  agents/openai.yaml
```

### Automatic discovery and explicit use

The YAML file under `agents/` supplies Codex UI metadata and permits automatic
invocation when an integration task matches the skill description. Codex discovers
repository skills from the working directory through the repository root. In the
CLI or IDE extension, select this skill with `$serializer-integration` or `/skills`.
See [Codex skill discovery](https://learn.chatgpt.com/docs/build-skills#where-codex-loads-local-skills).

Example request:

```text
Use $serializer-integration to add Serializer to my C++ application,
define a person schema with stable_ids, and decode integer-key binary
messages with explicit resource limits.
```

### Use from a consuming project

Codex's ancestor-directory scan does not automatically load a nested dependency's
skill from the consuming project's root. Use the [direct-path request in the README](../README.md#integrate-with-a-coding-agent), or
add this instruction to the application's own `AGENTS.md`, adapting the path:

```markdown
When integrating or changing Serializer usage, read and follow
vendor/Serializer/.agents/skills/serializer-integration/SKILL.md.
```

For skill-selector discovery in that project, copy the complete `serializer-integration`
folder into its `.agents/skills/`. Keep that copy aligned with the Serializer version in
use and retain the matching checkout for linked documentation. Other agents have their
own discovery rules; providing the direct file path lets them read the same instructions
without relying on automatic scanning.

These instructions accompany the source checkout; the CMake binary installation does not
currently install the skill or its documentation.

[AGENTS.md](../AGENTS.md#usage-documentation-and-repository-skill) requires changes to
keep the project README and the skill current together.

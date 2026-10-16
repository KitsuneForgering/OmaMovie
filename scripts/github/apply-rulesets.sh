#!/usr/bin/env bash
# Apply the repository's master ruleset through gh api. Every run snapshots the live state
# before comparing it; only the managed ruleset is created or updated.
set -euo pipefail

repo=KitsuneForgering/OmaMovie
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
desired="$root/.github/rulesets/master.json"
api="repos/$repo/rulesets"
backup=$(mktemp -d "${TMPDIR:-/tmp}/omamovie-rulesets.XXXXXX")
echo "Ruleset backup: $backup"

for tool in gh jq diff; do
    command -v "$tool" >/dev/null || { echo "Missing $tool" >&2; exit 1; }
done
jq -e '.name == "Protect master" and .target == "branch" and
       .enforcement == "active" and
       .conditions.ref_name.include == ["refs/heads/master"] and
       .conditions.ref_name.exclude == [] and
       ([.bypass_actors[] | .actor_type] == ["RepositoryRole"]) and
       ([.bypass_actors[] | .actor_id] == [5]) and
       ([.bypass_actors[] | .bypass_mode] == ["always"]) and
       (([.rules[].type] | sort) ==
        (["deletion", "non_fast_forward", "pull_request", "required_linear_history", "required_status_checks"] | sort)) and
       ([.rules[] | select(.type == "pull_request") | .parameters.required_review_thread_resolution] == [true]) and
       (([.rules[] | select(.type == "pull_request") | .parameters.allowed_merge_methods[]] |
          sort) == ["rebase", "squash"]) and
       ([.rules[] | select(.type == "required_status_checks") |
         .parameters.strict_required_status_checks_policy] == [true]) and
       (([.rules[] | select(.type == "required_status_checks") |
          .parameters.required_status_checks[].context] | sort) ==
        ["ci", "conventional commits"])' \
    "$desired" >/dev/null

default_branch=$(gh api "repos/$repo" --jq .default_branch)
[[ $default_branch == master ]] || {
    echo "Refusing to apply master.json: default branch is $default_branch" >&2
    exit 1
}

# List and save both effective (including organization) and repository-owned rulesets.
gh api "$api?includes_parents=true&per_page=100" >"$backup/effective.json"
gh api "$api?includes_parents=false&per_page=100" >"$backup/repository.json"
jq -e 'length < 100' "$backup/effective.json" "$backup/repository.json" >/dev/null || {
    echo "Ruleset list may be paginated; refusing an incomplete snapshot" >&2
    exit 1
}
echo "Current effective rulesets:"
jq -r '.[] | "  \(.id) \(.name) [\(.source_type)] \(.enforcement)"' "$backup/effective.json"

while IFS= read -r id; do
    gh api "$api/$id" >"$backup/repository-$id.json"
done < <(jq -r '.[].id' "$backup/repository.json")

ids=$(jq -r --arg name "$(jq -r .name "$desired")" \
    '[.[] | select(.name == $name) | .id] | join(" ")' "$backup/repository.json")
[[ $ids != *' '* ]] || { echo "Multiple managed rulesets found: $ids" >&2; exit 1; }

# Compare only the fields this repository owns; GitHub adds read-only metadata and defaults.
canonical='{
  name, target, enforcement,
  bypass_actors: ((.bypass_actors // []) | sort_by(.actor_type, .actor_id, .bypass_mode)),
  conditions: {ref_name: {
    include: (.conditions.ref_name.include | sort),
    exclude: (.conditions.ref_name.exclude | sort)
  }},
  rules: ([.rules[] |
    if .type == "pull_request" then
      {type, parameters: (.parameters | {
        allowed_merge_methods: (.allowed_merge_methods | sort),
        dismiss_stale_reviews_on_push, require_code_owner_review,
        require_last_push_approval, required_approving_review_count,
        required_review_thread_resolution
      })}
    elif .type == "required_status_checks" then
      {type, parameters: (.parameters | {
        required_status_checks: (.required_status_checks | sort_by(.context)),
        strict_required_status_checks_policy
      })}
    else {type} end] | sort_by(.type))
}'
jq -S "$canonical" "$desired" >"$backup/desired.normalized.json"

if [[ -z $ids ]]; then
    echo "Desired change: create Protect master"
    cat "$backup/desired.normalized.json"
    gh api --method POST "$api" --input "$desired" >"$backup/applied.json"
    id=$(jq -r .id "$backup/applied.json")
else
    id=$ids
    jq -e '.target == "branch" and
           .conditions.ref_name.include == ["refs/heads/master"] and
           .conditions.ref_name.exclude == []' "$backup/repository-$id.json" >/dev/null || {
        echo "Existing ruleset $id targets other refs; refusing to change it" >&2
        exit 1
    }
    jq -S "$canonical" "$backup/repository-$id.json" >"$backup/current.normalized.json"
    echo "Desired difference for ruleset $id:"
    if diff -u "$backup/current.normalized.json" "$backup/desired.normalized.json"; then
        echo "Ruleset already matches; no update sent"
        exit 0
    fi
    jq -n --slurpfile current "$backup/current.normalized.json" \
        --slurpfile wanted "$backup/desired.normalized.json" \
        --slurpfile raw "$desired" '
        reduce ["name", "target", "enforcement", "bypass_actors", "conditions", "rules"][] as $key
          ({}; if $current[0][$key] == $wanted[0][$key] then .
               else . + {($key): $raw[0][$key]} end)' >"$backup/changes.json"
    gh api --method PUT "$api/$id" --input "$backup/changes.json" >"$backup/applied.json"
fi

# Verify the server state, including the rule target, after any mutation.
gh api "$api/$id" >"$backup/verified.json"
jq -S "$canonical" "$backup/verified.json" >"$backup/verified.normalized.json"
diff -u "$backup/desired.normalized.json" "$backup/verified.normalized.json"
echo "Ruleset $id active and verified: https://github.com/$repo/settings/rules/$id"

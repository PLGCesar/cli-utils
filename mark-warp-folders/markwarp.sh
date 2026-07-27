#!/usr/bin/env bash

mark() {
    if [ -z "$1" ]; then
        echo "Usage: mark <name> [optional_path]"
        return 1
    fi
    markwarp mark "$@"
}

warp() {
    if [ -z "$1" ]; then
        markwarp list
        return 0
    fi

    local target_dir
    target_dir=$(markwarp get "$1" 2>/dev/null)

    if [ $? -eq 0 ] && [ -n "$target_dir" ]; then
        cd "$target_dir" || return 1
    else
        echo -e "\033[31mError: Mark '$1' not found.\033[0m"
        return 1
    fi
}

_warp_autocomplete() {
    local cur="${COMP_WORDS[COMP_CWORD]}"
    if [ -f "$HOME/.marks" ]; then
        local marks
        marks=$(cut -d'=' -f1 "$HOME/.marks" 2>/dev/null)
        COMPREPLY=( $(compgen -W "$marks" -- "$cur") )
    fi
}
complete -F _warp_autocomplete warp

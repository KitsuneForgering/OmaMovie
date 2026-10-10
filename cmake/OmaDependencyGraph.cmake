# The dependency graph from CLAUDE.md §5.2, enforced at configure time (ADR-0018). A forbidden
# dependency or an unregistered lib fails configuration before anything compiles.
set(ALLOWED_DEPS_base "")
set(ALLOWED_DEPS_gpu base)
set(ALLOWED_DEPS_media base gpu)
set(ALLOWED_DEPS_compositor base gpu media)
set(ALLOWED_DEPS_audio base)
set(ALLOWED_DEPS_timeline base)
set(ALLOWED_DEPS_playback base gpu media audio timeline)
set(ALLOWED_DEPS_project-ir base)
set(ALLOWED_DEPS_importers base project-ir)
set(ALLOWED_DEPS_project base timeline project-ir)

function(oma_enforce_deps name)
    if(NOT DEFINED ALLOWED_DEPS_${name})
        message(FATAL_ERROR
            "lib '${name}' is not registered in the build (ALLOWED_DEPS_${name}); see CLAUDE.md §5.2")
    endif()
    set(allowed ${ALLOWED_DEPS_${name}})
    foreach(dep ${ARGN})
        if(NOT dep IN_LIST allowed)
            message(FATAL_ERROR
                "lib '${name}': forbidden dependency on '${dep}'. Allowed: [${allowed}] (CLAUDE.md §5.2)")
        endif()
    endforeach()
endfunction()

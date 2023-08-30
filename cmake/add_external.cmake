include(FetchContent)

macro(add_external target git_repo git_tag)
  FetchContent_Declare(
    ${target}
    GIT_REPOSITORY "${git_repo}"
    GIT_TAG ${git_tag}
  )
  FetchContent_MakeAvailable(${target})
endmacro()
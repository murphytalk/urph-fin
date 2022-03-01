# Remove strings matching given regular expression from a list.
# @param(in,out) aItems Reference of a list variable to filter.
# @param aRegEx Value of regular expression to match.
function (filter_items aItems aRegEx)
    # For each item in our list
    foreach (item ${${aItems}})
        # Check if our items matches our regular expression
        if ("${item}" MATCHES ${aRegEx})
            # Remove current item from our list
            list (REMOVE_ITEM ${aItems} ${item})
        endif ("${item}" MATCHES ${aRegEx})
    endforeach(item)
    # Provide output parameter
    set(${aItems} ${${aItems}} PARENT_SCOPE)
endfunction (filter_items)

macro(add_src_libs_)
  ################################
  # Include all c++ source files
  ################################
  unset(the_SRC)
  file(GLOB_RECURSE the_SRC
    "src/[a-zA-Z]*.cpp"
    "src/[a-zA-Z]*.cc"
    )
  #filter_items(the_SRC "\.cquery_cached_index")
endmacro()

function(add_log_header t)
  target_include_directories(${t} PRIVATE ${project_root}/3rd-party/aixlog/include)
endfunction(add_log_header)

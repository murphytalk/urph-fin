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

function(add_src_libs_ dir src_list)
  ################################
  # Include all c++ source files
  ################################
  file(GLOB_RECURSE files
    "${dir}/[a-zA-Z]*.cpp"
    "${dir}/[a-zA-Z]*.cc"
    )
  #filter_items(the_SRC "\.cquery_cached_index")
  set("${src_list}" "${files}" PARENT_SCOPE)
endfunction()

# OTS_LITE stand-in for art's Modules/art::plugin.cmake (same macro, without the art
# link dependencies). cet_write_plugin_builder() generates builder files that do
# include(art::plugin), so this file must exist under that exact name on the module
# path. The SUFFIX is honored: at runtime cetlib::BasicPluginFactory("<suffix>", ...)
# looks up lib<Name>_<suffix>.so.
include_guard()

include(BasicPlugin)

macro(art::plugin NAME)
  cmake_parse_arguments(_ap "" "SUFFIX" "" ${ARGN})
  if ("${_ap_SUFFIX}" STREQUAL "")
    set(_ap_SUFFIX plugin)
  endif()
  basic_plugin(${NAME} ${_ap_SUFFIX} ${_ap_UNPARSED_ARGUMENTS}
    LIBRARIES CONDITIONAL fhiclcpp::fhiclcpp cetlib::cetlib cetlib_except::cetlib_except)
  unset(_ap_SUFFIX)
  unset(_ap_UNPARSED_ARGUMENTS)
endmacro()

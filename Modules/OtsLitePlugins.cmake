# OTS_LITE replacement for art's BuildPlugins.cmake.
# Mirrors art's Modules/art::plugin.cmake without the art link dependencies, so the
# cet_write_plugin_builder(otsdaq::<type> art::plugin ... SUFFIX <suffix>) calls in
# otsdaq work unchanged without art installed. The SUFFIX must be honored: at runtime
# cetlib::BasicPluginFactory("<suffix>", ...) looks up lib<Name>_<suffix>.so.
include_guard()

include(BasicPlugin)

macro(art::plugin NAME)
  cmake_parse_arguments(_ap "" "SUFFIX" "" ${ARGN})
  if ("${_ap_SUFFIX}" STREQUAL "")
    set(_ap_SUFFIX plugin)
  endif()
  basic_plugin(${NAME} ${_ap_SUFFIX} ${_ap_UNPARSED_ARGUMENTS}
    LIBRARIES CONDITIONAL fhiclcpp::fhiclcpp cetlib::cetlib cetlib_except::cetlib_except)
endmacro()

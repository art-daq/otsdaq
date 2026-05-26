
cet_make_exec(NAME otsdaq_fix_new_table_fields_tool LIBRARIES otsdaq::ConfigurationInterface)
cet_make_exec(NAME otsdaq_flatten_system_aliases LIBRARIES otsdaq::ConfigurationInterface)
# cet_make_exec(NAME otsdaq_import_system_aliases LIBRARIES otsdaq::ConfigurationInterface) #DEPRECATED in favor of more generic otsdaq/tools/otsdaq_import_system_aliases.sh which uses otsdaq_export_system_aliases/otsdaq_import_groups_from_export_path

cet_make_exec(NAME otsdaq_export_system_aliases LIBRARIES otsdaq::ConfigurationInterface)
cet_make_exec(NAME otsdaq_import_groups_from_export_path LIBRARIES otsdaq::ConfigurationInterface)


cet_make_exec(NAME otsdaq_save_json_document LIBRARIES otsdaq::ConfigurationInterface)
cet_make_exec(NAME otsdaq_load_json_document LIBRARIES otsdaq::ConfigurationInterface)


cet_script(ALWAYS_COPY
    ots_common.sh
    ots_compile_aliases.sh
    EclipseBuild.sh
    ots
    ots_remote_start
    ots_remote_instance_start
    otsdaq_add_artdaq_tables
    otsdaq_beautify_artdaq_tables
    otsdaq_convert_artdaq_daq_tables
    # otsdaq_convert_config_to_table #DEPRECATED
    otsdaq_import_system_aliases.sh
    otsweb_login.sh
    redmine_login.sh
    reset_ots_snapshot.sh
    StartOTS.sh
    vless_ots.sh
    vtail_ots.sh
    otsdaq_cmdline.py
    send_gateway_command.py
)

install_fhicl(SUBDIRS fcl)
install_headers()
install_source()

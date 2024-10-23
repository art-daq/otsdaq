
cet_make_exec(NAME otsdaq_fix_new_table_fields_tool LIBRARIES otsdaq::ConfigurationInterface)
cet_make_exec(NAME otsdaq_flatten_system_aliases LIBRARIES otsdaq::ConfigurationInterface)
cet_make_exec(NAME otsdaq_import_system_aliases LIBRARIES otsdaq::ConfigurationInterface)

cet_make_exec(NAME otsdaq_save_json_document LIBRARIES otsdaq::ConfigurationInterface)
cet_make_exec(NAME otsdaq_load_json_document LIBRARIES otsdaq::ConfigurationInterface)


cet_script(ALWAYS_COPY 
    common.sh 
    EclipseBuild.sh 
    ots 
    ots_remote_start 
    ots_remote_instance_start 
    otsdaq_add_artdaq_tables 
    otsdaq_beautify_artdaq_tables 
    otsdaq_convert_artdaq_daq_tables 
    otsdaq_convert_config_to_table 
    otsweb_login.sh 
    redmine_login.sh 
    reset_ots_snapshot.sh 
    StartOTS.sh 
    vless_ots.sh
    vtail_ots.sh)

install_fhicl(SUBDIRS fcl)
install_headers()
install_source()


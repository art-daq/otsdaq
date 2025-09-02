[![otsdaq](https://github.com/art-daq/otsdaq/blob/develop/doc/logo.png?raw=true)](https://otsdaq.fnal.gov/)

otsdaq is a Ready-to-Use data-acquisition (DAQ) solution aimed at test-beam, detector development, and other rapid-deployment scenarios. otsdaq uses the artdaq DAQ framework under-the-hood, providing flexibility and scalability to meet evolving DAQ needs.

otsdaq provides a library of supported front-end boards and firmware modules which implement a custom UDP protocol. Additionally, an integrated Run Control GUI and readout software are provided, preconfigured to communicate with otsdaq firmware.

# Code Documentation {#topicList}


Below are some helpful links to _otsdaq_ C++ classes organized by topic. The topics are as follows:  
  
*   [Supervisor Base Classes](#supervisor)
*   [_otsdaq_ Core Supervisors](#coreSupervisors)
*   [Configuration Primer](https://otsdaq.fnal.gov/tutorials/first_demo/topics/configuration_primer.html)
*   [Web Desktop Login and Requests](#access)
*   [Front-ends](#frontends)
*   [Data Managers](#dataManagers)
*   [_artdaq_](#artdaq)
*   [Visualization and DQM](#visualization)
*   [Slow Controls](#slowControls)

  
  
_otsdaq_ is composed of three core repositories (otsdaq, otsdaq-utilities, and otsdaq-components) and one example user repository (otsdaq-demo). The intention is for users to clone otsdaq-demo into one or many of their own repositories for their own specific applications. Here are the links to the source code documentation for each respository:  
  

*   [otsdaq](https://art-daq.github.io/otsdaq_doxygen/otsdaq/)
*   [otsdaq-utilities](https://art-daq.github.io/otsdaq_doxygen/otsdaq-utilities/)
*   [otsdaq-components](https://art-daq.github.io/otsdaq_doxygen/otsdaq-components/)
*   [otsdaq-demo](https://art-daq.github.io/otsdaq_doxygen/otsdaq-demo/)

  
  
_otsdaq_ is built on top of the _artdaq_ toolkit and the XDAQ toolkit:  
  

*   [_artdaq_ homepage](https://artdaq.fnal.gov)
*   [XDAQ homepage](https://twiki.cern.ch/twiki/bin/view/CMSPublic/CMSOS)
*   [_otsdaq_ homepage](https://otsdaq.fnal.gov)

  

* * *

  

  
  
[Jump to Topics List](#topicList)

Supervisor Base Classes {#supervisor}
=======================

All client otsdaq supervisors should inherit functionality from these classes. Inheriting from CoreSupervisorBase should be sufficient for most user-created supervisors for compatibility within otsdaq.

Class Name and Link

Brief Description

[CoreSupervisorBase](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_core_supervisor_base.html)

This class should be the base class for all client otsdaq, XDAQ-based, supervisors. That is, all supervisors that need web requests through the ots desktop with access verified by the Gateway Supervisor, or that need a state machines driven by the Gateway Supervisor.

[xdaq::Application](https://twiki.cern.ch/twiki/bin/view/CMSPublic/CMSOS)

This class provides the XDAQ functionality for otsdaq supervisors, such as inter-process communication and web request binding to C++ handlers.

[CorePropertySupervisorBase](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_core_property_supervisor_base.html)

This class provides supervisor property get and set functionality. It has member variables generally useful to the configuration of client supervisors.

[RunControlStateMachine](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_run_control_state_machine.html)

This class provides finite state machine functionality for otsdaq supervisors.

  
  
[Jump to Topics List](#topicList)

otsdaq Core Supervisors {#coreSupervisors}
=========================

The otsdaq Core Supervisors are the supervisors provided with otsdaq and otsdaq-utilities distribution.

Class Name and Link

Brief Description

[GatewaySupervisor](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_gateway_supervisor.html)

This class is the gateway server for all otsdaq requests in "Normal Mode." It validates user access for every request. It synchronizes the state machines of all other superviso

[WizardSupervisor](#WizardSupervisor)

This class is a xdaq application. It is instantiated by the xdaq context when otsdaq is in "Wiz Mode." It is different from the "Normal Mode" Gateway Supervisor in that it does not have a state machine and does not inherit properties from CorePropertySupervisorBase. The assumption is that only admins have access to wiz mode, and they have access to all features of it.

[FESupervisor](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_f_e_supervisor.html)

This class handles a collection of front-end interface pluginss. It provides an interface to Macro Maker for writes and reads to the front-end interfaces.

[DataManagerSupervisor](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_data_manager_supervisor.html)

This class handles a collection of Data Processor plugins. It provides a mechanism for Data Processor Producers to store data in Buffers, and for Data Processor Consumers to retrive data from the Buffers.

[ChatSupervisor](https://art-daq.github.io/otsdaq_doxygen/otsdaq-utilities/classots_1_1_chat_supervisor.html)

This class handles the otsdaq user chat functionality available in the web desktop environment.

[ConfigurationGUISupervisor](https://art-daq.github.io/otsdaq_doxygen/otsdaq-utilities/classots_1_1_configuration_g_u_i_supervisor.html)

This class handles the user requests to read and write the Configuration Tree.

[ConsoleSupervisor](https://art-daq.github.io/otsdaq_doxygen/otsdaq-utilities/classots_1_1_console_supervisor.html)

This class handles the presentation of Message Facility printouts to the web desktop Console.

[SlowControlsDashboardSupervisor](https://art-daq.github.io/otsdaq_doxygen/otsdaq-utilities/classots_1_1_slow_controls_dashboard_supervisor.html)

This class handles the management of slow controls interface plugins, as well as the user web interface.

[LogbookSupervisor](https://art-daq.github.io/otsdaq_doxygen/otsdaq-utilities/classots_1_1_logbook_supervisor.html)

This class handles the write and read requests for web users interfacing to the web desktop Logbook.

[MacroMakerSupervisor](https://art-daq.github.io/otsdaq_doxygen/otsdaq-utilities/classots_1_1_macro_maker_supervisor.html)

This class handles the user interface to the web desktop MacroMaker. MacroMaker is a tool to conduct read and write commands with front-end interfaces and to manage sequence of commands on a per user basis.

[VisualSupervisor](https://art-daq.github.io/otsdaq_doxygen/otsdaq-utilities/classots_1_1_visual_supervisor.html)

This class handles the web user interface to a VisualDataManager with reqgard to the web desktop Visualizer. The Visualizer can display ROOT object in real-time, as well as 2D and 3D displays of streaming data.

  
  
[Jump to Topics List](#topicList)

Web Desktop Login and Requests {#access}
==============================

The classes in this section are involved in user account management and system security.

Class Name and Link

Brief Description

[GatewaySupervisor](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_gateway_supervisor.html)

This class is the gateway server for all otsdaq requests in "Normal Mode." It validates user access for every request. It synchronizes the state machines of all other superviso

[WebUsers](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_web_users.html)

This class provides the functionality for managing all otsdaq user account preferences and permissions, including password access and [CILogon](https://www.cilogon.org/) certificate access.

[RemoteWebUsers](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_remote_web_users.html)

This class provides the functionality for client supervisors to check with the Gateway Supervisor to verify user access. It also provides the functionality for client supervisors to retreive user info.

  
  
[Jump to Topics List](#topicList)

Front-ends {#frontends}
==========

The classes in this section are involved with the control and management of front-end interface. Front-end interfaces are considered to be the specifics for how to interface to a device external to otsdaq. For example, a front-end interface might interface to physics detector readout electronics or a detector readout software emulator.

Class Name and Link

Brief Description

[FESupervisor](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_f_e_supervisor.html)

This class handles a collection of front-end interface pluginss. It provides an interface to Macro Maker for writes and reads to the front-end interfaces.

[FEDataManagerSupervisor](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_f_e_data_manager_supervisor.html)

This class handles two collections: a collection of Front-end plugins, and a collection of Data Processor plugins (see [Data Managers](#dataManagers)). The unique functionality of the FEDataManagerSupervisor is if a FEProducerVInterface plugin is instantiated in the collection of Front-end plugins, then that FEProducerVInterface will also be included in the collection of Data Processor plugin as a Data Producer - thus creating a single plugin instance that is a hybrid between Front-end plugin and Data Producer. This may be useful if, for example, it is convenient for the front-end interface to also receive streaming data to be saved or monitored.

[FEVInterfacesManager](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_f_e_v_interfaces_manager.html)

This class is a virtual class that handles a collection of front-end interface plugins.

[FEVInterface](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_f_e_v_interface.html)

This class is a virtual class defining the features of front-end interface plugin class. The features include configuration hooks, finite state machine handlers, Front-end Macros for web accessible C++ handlers, slow controls hooks, as well as universal write and read for Macro Maker compatibility.

[FEProducerVInterface](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_f_e_producer_v_interface.html)

This class provides base class functionality for Front-end Data Producer plugin classes that interface to front-end devices and place incoming streaming data in a Buffer. This is a plugin base that class that is a hybrid between a FEVInterface and a DataProducerBase (see [Data Managers](#dataManagers)).

[FESlowControlsChannel](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_f_e_slow_controls_channel.html)

This class provides base slow controls channel functionality for Front-end plugin classes to monitor slow controls channels. Slow controls channels have fields like name, address, bit-field size, alarm thresholds, etc. (see [Slow Controls](#slowControls)).

  
  
[Jump to Topics List](#topicList)

Data Managers {#dataManagers}
=============

These classes are associated with the handling of data being received by the otsdaq system.

Class Name and Link

Brief Description

[DataManagerSupervisor](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_data_manager_supervisor.html)

This class handles a collection of Data Processor plugins. It provides a mechanism for Data Processor Producers to store data in Buffers, and for Data Processor Consumers to retrive data from the Buffers.

[FEDataManagerSupervisor](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_f_e_data_manager_supervisor.html)

This class handles two collections: a collection of Front-end plugins (see [Front-ends](#frontends)), and a collection of Data Processor plugins. The unique functionality of the FEDataManagerSupervisor is if a FEProducerVInterface plugin is instantiated in the collection of Front-end plugins, then that FEProducerVInterface will also be included in the collection of Data Processor plugin as a Data Producer - thus creating a single plugin instance that is a hybrid between Front-end plugin and Data Producer. This may be useful if, for example, it is convenient for the front-end interface to also receive streaming data to be saved or monitored.

[DataManager](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_data_manager.html)

This class is the base class that handles a collection of Buffers and associated Data Processor plugins.

[CircularBufferBase](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_circular_buffer_base.html)

This class is the base class for the otsdaq Buffer

[DataConsumer](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_data_consumer.html)

This class provides base class functionality for Data Consumer plugin classes to extracts and process streaming data from a Buffer.

[DataProcessor](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_data_processor.html)

This class provides common functionality for Data Producers and Consumers.

[DataProducerBase](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_data_producer_base.html)

This class provides base class functionality for Data Producer plugin classes to receive incoming streaming data and places it in a Buffer.

[DataProducer](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_data_producer.html)

This class provides adds workloop functionality for Data Producer plugin classes for running.

[FEProducerVInterface](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_f_e_producer_v_interface.html)

This class provides base class functionality for Front-end Data Producer plugin classes that interface to front-end devices and place incoming streaming data in a Buffer. This is a plugin base that class that is a hybrid between a FEVInterface (see [Front-ends](#frontends)) and a DataProducerBase.

  
  
[Jump to Topics List](#topicList)

artdaq {#artdaq}
========

The classes in this section are involved with the otsdaq layer on top of the artdaq toolkit.

Class Name and Link

Brief Description

[ARTDAQSupervisor](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_a_r_t_d_a_q_supervisor.html)

This class is the supervisor which handles interfacing to the artdaq DAQ interface which, in turn, manages all [artdaq](https://artdaq.fnal.gov) processes in a one or many node system. There can only be one ARTDAQSupervisor in your system. artdaq processes that are managed include Board Readers, Event Builders, Data Loggers, Dispatcher, Metric plugins, and Routing Masters. The artdaq Configuration editor, in conjunction with ARTDAQTableBase establish the configuration of artdaq processes for the ARTDAQSupervisor.

[ARTDAQTableBase](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_a_r_t_d_a_q_table_base.html)

This class provides the base functionality for artdaq configuration table plugins (i.e. ARTDAQBoardReaderTable, ARTDAQEventBuilderTable ARTDAQDataLoggerTable, ARTDAQDispatcherTable, ARTDAQRoutingMasterTable) to generate the configuration (including [FHiCL](https://cdcvs.fnal.gov/redmine/projects/fhicl/wiki)) of artdaq processes in the system.

  
  
[Jump to Topics List](#topicList)

Visualization and DQM {#visualization}
=====================

The classes in this section are involved with data visualization and Data Quality Monitoring (DQM).

Class Name and Link

Brief Description

[VisualSupervisor](https://art-daq.github.io/otsdaq_doxygen/otsdaq-utilities/classots_1_1_visual_supervisor.html)

This class is the web server to visualize objects found on disk and also the container of the VisualDataManager for serving live visualization objects during a finite state machine Run. Essentailly, this supervisor provides Data Quality Monitoring (DQM) features including [ROOT](https://root.cern.ch/) historgram visualization, as well as an ots custom protocol for 2-D and 3-D displays in the web browser.

[VisualDataManager](https://art-daq.github.io/otsdaq_doxygen/otsdaq-utilities/classots_1_1_visual_data_manager.html)

This class provides functionality for handling Visualizer data consumer plugin classes that can produce live visualization objects during a finite state machine Run. It inherits all Data Manager (see [Data Managers](#dataManagers)) functionality as well.

[RawDataVisualizerConsumer](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_raw_data_visualizer_consumer.html)

This class provides example functionality for creating live visualization objects from the incoming data. It inherits all functionality of a DataConsumer plugin class (see [Data Managers](#dataManagers)). Custom visualizer plugins can be created using this class as an example, and can be children plugins to the VisualDataManager container. An example DQM use case is to create a live ROOT TTree based on incoming data, then users can use the Visualizer web-app to browser live data during a finite state machine Run.

  
  
[Jump to Topics List](#topicList)

Slow Controls {#slowControls}
=============

The classes in this section are involved with slow controls and monitoring. Generally, slow controls refers to a management and archiving system involving many channels. [EPICS](https://epics-controls.org/) is one such slow controls system. Slow controls channels are usually treated independently. Channels have associated fields (e.g. name, value, timestamp, alarm thresholds, etc.) and persistent history. In EPICS, a “Channel” is known as a “Process Variable” (PV), so PV and channel are often used interchangeably.

Class Name and Link

Brief Description

[SlowControlsDashboardSupervisor](https://art-daq.github.io/otsdaq_doxygen/otsdaq-utilities/classots_1_1_slow_controls_dashboard_supervisor.html)

This class is the web server and also the container of a SlowControlsVInterface plugin for serving slow controls channels to the Slow Controls Dashboard web app.

[SlowControlsVInterface](https://art-daq.github.io/otsdaq_doxygen/otsdaq/classots_1_1_slow_controls_v_interface.html)

This class establishes the base functionality for slow controls interface plugins, which are responsible for handling slow controls channels within the SlowControlsDashboardSupervisor. Channels (or PVs) have a notion of subscription, current values, and historical values.

[OtsSlowControlsInterface](https://art-daq.github.io/otsdaq_doxygen/otsdaq-components/class_ots_slow_controls_interface.html)

This class is an example slow controls interface plugin inheriting functionality from the SlowControlsVInterface. It implements a custom ots slow controls protocol for saving and retrieving channel values.

[EpicsInterface](https://art-daq.github.io/otsdaq_doxygen/otsdaq-epics/classots_1_1_epics_interface.html)

This class is the EPICS slow controls interface plugin inheriting functionality from the SlowControlsVInterface. It implements a the EPICS channel access protocol for handling PVs.

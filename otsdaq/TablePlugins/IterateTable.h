#ifndef _ots_IterateTable_h_
#define _ots_IterateTable_h_

#include "otsdaq/TableCore/TableBase.h"

namespace ots
{
class IterateTable : public TableBase
{
  public:
	IterateTable(void);
	virtual ~IterateTable(void);

	// Methods
	void init(ConfigurationManager* configManager);

	// Getters

	struct CommandTarget
	{
		std::string table_;
		std::string UID_;
	};

	struct Command
	{
		void                                                              addTarget() { targets_.push_back(CommandTarget()); }
		std::string                                                       type_;
		std::vector<CommandTarget>                                        targets_;
		std::map<std::string /*param name*/, std::string /*param value*/> params_;
	};

	std::vector<IterateTable::Command> getPlanCommands(ConfigurationManager* configManager, const std::string& plan) const;

	static const std::string COMMAND_BEGIN_LABEL;
	static const std::string COMMAND_CHOOSE_FSM;
	static const std::string COMMAND_CONFIGURE_ACTIVE_GROUP;
	static const std::string COMMAND_CONFIGURE_ALIAS;
	static const std::string COMMAND_CONFIGURE_GROUP;
	static const std::string COMMAND_ACTIVATE_ALIAS;
	static const std::string COMMAND_ACTIVATE_GROUP;
	static const std::string COMMAND_EXECUTE_FE_MACRO;
	static const std::string COMMAND_EXECUTE_MACRO;
	static const std::string COMMAND_MODIFY_ACTIVE_GROUP;
	static const std::string COMMAND_REPEAT_LABEL;
	static const std::string COMMAND_RUN;
	static const std::string COMMAND_START;
	static const std::string COMMAND_STOP;
	static const std::string COMMAND_PAUSE;
	static const std::string COMMAND_RESUME;
	static const std::string COMMAND_HALT;

	static const std::string ITERATE_TABLE;
	static const std::string PLAN_TABLE;
	static const std::string TARGET_TABLE;

	static const std::map<std::string, std::string> commandToTableMap_;
	static std::map<std::string, std::string>       createCommandToTableMap()
	{
		std::map<std::string, std::string> m;
		m[COMMAND_BEGIN_LABEL]            = "IterationCommandBeginLabelTable";
		m[COMMAND_CHOOSE_FSM]             = "IterationCommandChooseFSMTable";
		m[COMMAND_CONFIGURE_ACTIVE_GROUP] = "";  // no parameters
		m[COMMAND_CONFIGURE_ALIAS]        = "IterationCommandConfigureAliasTable";
		m[COMMAND_CONFIGURE_GROUP]        = "IterationCommandConfigureGroupTable";
		m[COMMAND_ACTIVATE_ALIAS]         = "IterationCommandConfigureAliasTable";
		m[COMMAND_ACTIVATE_GROUP]         = "IterationCommandConfigureGroupTable";
		m[COMMAND_EXECUTE_FE_MACRO]       = "IterationCommandExecuteFEMacroTable";
		m[COMMAND_EXECUTE_MACRO]          = "IterationCommandExecuteMacroTable";
		m[COMMAND_MODIFY_ACTIVE_GROUP]    = "IterationCommandModifyGroupTable";
		m[COMMAND_REPEAT_LABEL]           = "IterationCommandRepeatLabelTable";
		m[COMMAND_RUN]                    = "IterationCommandRunTable";
		m[COMMAND_START]                  = "";  // no parameters
		m[COMMAND_STOP]                   = "";  // no parameters
		m[COMMAND_PAUSE]                  = "";  // no parameters
		m[COMMAND_RESUME]                 = "";  // no parameters
		m[COMMAND_HALT]                   = "";  // no parameters
		return m;
	}

	static struct CommandBeginLabelParams
	{
		const std::string Label_ = "Label";
	} commandBeginLabelParams_;
	static struct CommandChooseFSMParams
	{
		const std::string NameOfFSM_ = "NameOfStateMachine";  // by default ""
	} commandChooseFSMParams_;
	static struct CommandConfigureActiveParams
	{
		// no parameters
	} commandConfigureActiveParams_;
	static struct CommandConfigureAliasParams
	{
		const std::string SystemAlias_ = "SystemAlias";
	} commandConfigureAliasParams_;
	static struct CommandConfigureGroupParams
	{
		const std::string GroupName_ = "GroupName";
		const std::string GroupKey_  = "GroupKey";
	} commandConfigureGroupParams_;
	static struct CommandActivateAliasParams
	{
		const std::string SystemAlias_ = "SystemAlias";
	} commandActivateAliasParams_;
	static struct CommandActivateGroupParams
	{
		const std::string GroupName_ = "GroupName";
		const std::string GroupKey_  = "GroupKey";
	} commandActivateGroupParams_;
	static struct CommandExecuteMacroParams  // treat FE and Macro the same
	{
		// targets
		const std::string MacroName_          = "MacroName";
		const std::string MacroParameterLink_ = "LinkToMacroDimensionalLoopTable";
		const std::string EnableSavingOutput_ = "EnableSavingOutputsToFile";
		const std::string OutputFilePath_     = "OutputFilePath";
		const std::string OutputFileRadix_    = "OutputFileRadix";

		const std::string MacroArgumentString_ = "MacroArgumentString";

		// macro parameters
	} commandExecuteMacroParams_;
	static struct CommandModifyActiveParams
	{
		// targets
		const std::string DoTrackGroupChanges_    = "DoTrackGroupChanges";
		const std::string RelativePathToField_    = "RelativePathToField";
		const std::string FieldStartValue_        = "FieldStartValue";
		const std::string FieldIterationStepSize_ = "FieldIterationStepSize";
	} commandModifyActiveParams_;
	static struct CommandRepeatLabelParams
	{
		const std::string Label_               = "Label";
		const std::string NumberOfRepetitions_ = "NumberOfRepetitions";
	} commandRepeatLabelParams_;
	static struct CommandRunParams
	{
		const std::string WaitOnRunningThreads_ = "WaitForAllFrontEndsRunningThread";
		const std::string DurationInSeconds_    = "DurationInSeconds";
	} commandRunParams_;

	// for targets
	static struct TargetParams
	{
		const std::string Tables_ = "CSVTargetTables";
		const std::string UIDs_   = "CSVTargetUIDs";
	} targetParams_;
	static struct CommandTargetColumns
	{
		const std::string TargetsLink_        = "LinkToTargets";
		const std::string TargetsLinkGroupID_ = "LinkToTargetsGroupID";

	} commandTargetCols_;
	static struct TargetTableColumns
	{
		const std::string GroupID_       = "IterationTargetGroupID";
		const std::string TargetLink_    = "LinkToTarget";
		const std::string TargetLinkUID_ = "LinkToTargetUID";
	} targetCols_;

	// for macro dimensional loop parameters
	static struct MacroDimLoopTableColumns
	{
		const std::string Priority_           = "DimensionalLoopPriority";
		const std::string NumberOfIterations_ = "NumberOfIterations";
		const std::string ParamLink_          = "LinkToDimensionalLoopParameterTable";
	} macroDimLoopCols_;
	static struct MacroParamTableColumns
	{
		const std::string Name_  = "ParameterName";
		const std::string Value_ = "ParameterInitialValue";
		const std::string Step_  = "ParameterStepSize";

	} macroParamCols_;

	// Table hierarchy is as follows:
	//	Iterate
	//		|- Plan
	//			|-Command Type 1
	//			|-Command Type 2 ...
	//			|-Command Type n

	// Column names

	static struct IterateTableColumns
	{
		const std::string PlanLink_ = "LinkToIterationPlanTable";
	} iterateTableCols_;

	static struct PlanTableColumns
	{
		const std::string Status_      = TableViewColumnInfo::COL_NAME_STATUS;
		const std::string GroupID_     = "IterationPlanGroupID";
		const std::string CommandLink_ = "LinkToCommandUID";
		const std::string CommandType_ = "CommandType";
	} planTableCols_;
};
}  // namespace ots
#endif

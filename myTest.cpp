/*********************************************************************************
 *                                                                              *
 *  COPYRIGHT (c) 2013 BPCE-SF.                                                 *
 *                                                                              *
 *  All rights reserved. This software is only to be used for the purpose for   *
 *  which it has been provided. No part of it is to be reproduced, transmitted, *
 *  stored in a retrieval system nor translated in any human or computer        *
 *  language in any way or for any other purposes whatsoever without the prior  *
 *  written consent of BPCE-SF.                                                 *
 *                                                                              *
 ********************************************************************************/

// #include <xml/xml_entity_utils.h>

// #include "../../../common/libsrc/pugixml/pugixml.h"

#include "emir_report.h"
#include "dbmappingapi/su_dbmapping.h"
#include "ChROME/entitywrapper.h"
#include <dbwrapperfuncs.h>

#include "stknsstruct.h"

#define EPSILON 0.02

using namespace std;
using namespace pugi;

void EmirReport::help()
{
	std::cout << "\n"
			  << "+------------------------------------------------------------------------------------------+\n"
			  << "|                      Rapport EMIR                                                        |\n"
			  << "|------------------------------------------------------------------------------------------|\n"
			  << "| Option  Arg          Definition                                       Required Defaut    |\n"
			  << "|==========================================================================================|\n"
			  << "|                                                                                          |\n"
			  << "| -H                   Help screen                                      Non      <OFF>     |\n"
			  << "|                                                                                          |\n"
			  << "| -F      Filter       Name of the trade filter to use ( mandatory      Oui                |\n"
			  << "|                      when 052 message type )                                             |\n"
			  << "|                                                                                          |\n"
			  << "| -TYPE   IR/FX/EQ     Financial Instrument Type                        Oui                |\n"
			  << "|                                                                                          |\n"
			  << "| -O      Fichier      Output file name that will be saved              Oui                |\n"
			  << "|                      SUMMITSPOOLDIR                                                      |\n"
			  << "|                                                                                          |\n"
			  << "| -OCODE      OCODE    OCODE                                            Oui      <OCODE>   |\n"
			  << "|                                                                                          |\n"
			  << "| -VAL                 To report valuation                              Non      <OFF>     |\n"
			  << "|                                                                                          |\n"
			  << "| -DATE   Date         Used as application date when running backdated  Non      <AsOf>    |\n"
			  << "|                      Format YYYYMMDD                                                     |\n"
			  << "|                      By defaut the AsOfDate is used                                      |\n"
			  << "|                                                                                          |\n"
			  << "| -DEBUG               Used outside of production for readability       Non                |\n"
			  << "|                                                                                          |\n"
			  << "| -SIM                 Mode simulation used outside of production       Non                |\n"
			  << "|                      in order not to write in the database                               |\n"
			  << "|                                                                                          |\n"
			  << "| -HIST                                                                 Non                |\n"
			  << "|                                                                                          |\n"
			  << "+------------------------------------------------------------------------------------------+\n\n"
			  << "Exemples:\n"
			  << "% genEmirXML  -F 0000472805 -DATE 20240329 -TYPE FX -OCODE 5BP1 -BMSGID -VAL 202404020000000 -O /logs/users/admaderdouri/spool/myRefit.xml\n\n";
}

int *EmirReport::bools()
{
	static int bools[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	return bools;
}

char **EmirReport::flags()
{
	static char *flags[] = {"H", "F", "O", "DATE", "TYPE", "OCODE", "BMSGID", "VAL", "DEBUG", "SIM", "HIST", "PM", (char *)NULL};
	return flags;
}

char **EmirReport::values()
{
	static char *values[] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
	return values;
}

EmirReport::~EmirReport()
{
}

void EmirReport::Init()
{
	char filterName[30] = {0};
	char outputFile[256] = {0};
	char date[12] = {0};
	char instType[3] = {0};
	char ocode[5] = {0};
	char bizMsgIdr[50] = {0};

	values()[1] = filterName;
	values()[2] = outputFile;
	values()[3] = date;
	values()[4] = instType;
	values()[5] = ocode;
	values()[6] = bizMsgIdr;

	try
	{
		SummitApplicationFrameWork::init(sEXCLUDE_CHECKACCESS);
	}
	catch (SummitException &ex)
	{
		throw SummitFatalException(ex.what());
	}

	if (bools()[0]) // HELP
	{
		help();
		throw SummitNonFatalException("Quit Program !");
	}

	isValuation = bools()[7]; // compute valuation report

	// Set debug mode
	Debug = bools()[8]; // DEBUG

	// Set simulation mode
	Sim = bools()[9]; // SIM

	isHIST = bools()[10];

	// Set filterName
	if (sIsBlank(filterName) != sTRUE)
	{
		FilterName = string(filterName);
		try
		{
			LoadFilter();
		}
		catch (SummitFatalException &ex)
		{
			throw SummitFatalException(ex.what());
		}
	}
	else
	{
		throw SummitFatalException("Option -F is mandatory !");
	}

	// Set outputFile
	if (getenv("SUMMITSPOOLDIR") == NULL)
	{
		throw SummitFatalException("Environment variable SUMMITSPOOLDIR is not set !");
	}

	if (sIsBlank(outputFile) != sTRUE)
	{
		if (strchr(outputFile, '/') || strchr(outputFile, '\\') || strchr(outputFile, '~'))
		{
			OutputFileName = string(outputFile);
		}
		else
		{
			OutputFileName = string(getenv("SUMMITSPOOLDIR")) + "/" + string(outputFile);
		}
	}
	else
	{
		char buf[9] = {0};
		sSDATE sdate = sISDate(Date);
		sprintf(buf, "%04d%02d%02d", sdate.Year, sdate.Month, sdate.Day);

		OutputFileName = string(getenv("SUMMITSPOOLDIR")) + "/" + "SFTR_" + buf + ".xml";
	}

	// Set date
	if (sIsBlank(date) != sTRUE)
	{
		// YYYYMMDD --> DD/MM/YY
		std::string strDate(date);
		std::string strDateDDMMYY = strDate.substr(6, 2) + '/' + strDate.substr(4, 2) + '/' + strDate.substr(2, 2);
		Date = sCIDate(strDateDDMMYY.c_str());

		if (Date == sMAXUS)
		{
			string ErrMsg = string("Invalid date: ") + date;
			throw SummitFatalException(ErrMsg);
		}

		// Date can't be in the future
		if (Date > sAppDate())
		{
			Date = sAppDate();
			Time = sSysTime();
		}

		if (Date < sAppDate())
		{
			Time = sCITime("235959");
		}
	}
	else
	{
		Date = sAppDate();
		Time = sSysTime();
	}

	// Set instrument type
	if (sIsBlank(instType) != sTRUE)
	{
		InstrumentType = string(instType);

		if (InstrumentType != "IR" && InstrumentType != "FX" && InstrumentType != "EQ")
		{
			throw SummitFatalException("Option -TYPE argument should be set to IR, FX or EQ !");
		}
	}
	else
	{
		throw SummitFatalException("Option -TYPE is mandatory !");
	}

	OCode = "OCOD";
	if (sIsBlank(ocode) != sTRUE)
	{
		OCode = string(ocode);
	}

	/*
	BizMsgIdr = "BIZMSGIDR";
	if (sIsBlank(bizMsgIdr) != sTRUE ) {
		BizMsgIdr = string(bizMsgIdr);
	}
	*/

	// set iFTermBKVDays OffSet from Gateway Data Mapping EMIR_GAPS
	// Offset Value from GapType=GAPFTERM
	char ftermBKVDay[20] = {0};
	SU_DBMapping *dbmapping = SU_DBMapping::GetDBMapping();
	const std::string mapName = "EMIR_GAPS";
	if (dbmapping->AddMapDefinitions(mapName.c_str(), "ALL") != sSUCCESS)
	{
		iFTermBKVDays = 0;
		std::ostringstream exceptMsg;
		exceptMsg << "Unable to load Data Mapping for [" << mapName << "] , default gaps to 0";
		sLogMessage(exceptMsg.str(), sLOG_INFO, 0);
	}
	else
	{
		char Gap[sMAX_TXT + 1] = {0};
		string GapType = "GAPFTERM";
		try
		{
			dbmapping->sDataMapNN(mapName.c_str(), "ALL", GapType.c_str(), Gap);
			// on error atoi will return 0
			iFTermBKVDays = atoi(Gap);
		}
		catch (...)
		{
			LOG_INFO("Unable to find target value in [" << mapName << "] mapping for GapType [" << GapType << "] Gap will be " << iFTermBKVDays);
		}
	}

	// Reporting date and time
	AppDate = sSysDate();
	AppTime = sSysTime();

	// Init exit trade list
	sInitList(&EmirExitTradesList.List, "cEMRTRACT");

	ChROME::EntityWrapper<sNSALARM> nsalarm("NSALARM");
	string query = "where NSALARMID = 'REFIT2' and AUDIT_CURRENT = 'Y'";
	try
	{
		bool found = nsalarm.sqlRead(query, sRM_LATEST_MODE, true);
		if (found)
		{
			NDALARMTIME = nsalarm->NSAlarmTime.dtdays; // sCIDate("24/04/2024")
		}
	}
	catch (...)
	{
		NDALARMTIME = sMAXUS;
	}
}

void EmirReport::LoadFilter()
{
	Filter = auto_ptr<SummitInstance<sTRADE_FILTER>>(new SummitInstance<sTRADE_FILTER>("FILTER"));

	if (!Filter.get())
	{
		string ErrMsg = "Error initializing filter";
		throw SummitFatalException(ErrMsg);
	}

	strcpy((*Filter)->Name.Name, const_cast<char *>(FilterName.c_str()));

	if ((*Filter).readOne(sRM_LATEST_MODE, false) != sSUCCESS)
	{
		SummitInstance<sSTRENT> desc("strent");
		string strMessage("readOne: failed for following item: ");
		strMessage += sEntityKeysToStr((*Filter).entity(), (*Filter).instance(), desc.instance());
		throw SummitFatalException(strMessage);
	}
}

/*sINT GetTradeVersionToExit(cEMIRDEC * emirExit)
{
	sINT VersionToExit = 0;
	sVALLIST valList;
	memset(&valList, 0, sizeof(sVALLIST));

	std::ostringstream sqlRequest ;
	sqlRequest << "SELECT Audit_Version FROM dmENV_HIST "
			   << "WHERE 1=1 "
			   << "and dmOwnerTable='"<<  ChROME::trim(ChROME::toString(sTTypeToTable(emirExit->TradeType), "sTABLE") )<< "' "
		   << "and TradeId='"<< emirExit->TradeId.Num << "' "
			   << "and Audit_Version >= "<< emirExit->TradeVersion;

	char sql[4096] = "\0";
	strcpy(sql, sqlRequest.str().c_str());
	sLogMessage("GetTradeVersionToExit sqlRequest : '%s'", sLOG_INFO, 0, sql);

	if ( sDBValidList(&valList, sql, NULL, NULL, 02|010|sDB_SERVER_RECONNECT) == sSUCCESS && valList.List.ItemsUsed > 0 )
	{
		for (int i=0; i < valList.List.ItemsUsed; i++)
		{
		std::string Audit_VersionStr = valList.Data[i].Row;
			VersionToExit = atoi(Audit_VersionStr.c_str());
		break;
		}
	}
	sLogMessage("GetTradeVersionToExit Version=%d" , sLOG_INFO,0,VersionToExit);
	return VersionToExit;

}
*/
bool removenode(xml_node &documentnode)
{
	bool isempty = false;
	for (xml_node child = documentnode.first_child(); child;)
	{
		xml_node next = child.next_sibling();
		xml_node parent = child.parent();

		if (child.first_child() == NULL)
		{
			if (string(child.value()).empty())
			{
				parent.remove_child(child);
				child = next;
				isempty = true;
				continue;
			}
		}
		else
		{
			removenode(child);

			if ((child.first_child() == NULL) && string(child.value()).empty())
			{
				parent.remove_child(child);
			}
		}

		child = next;
	}

	return isempty;
}

string EmirReport::computeBZID(string &bzId, const string &tradeid, int tradeversion, bool isvaluation, bool isHIST)
{
	bzId = "";
	if (isHIST)
	{
		bzId.append("HIST");
	}
	AppDate = sSysDate();
	AppTime = sSysTime();
	string datetmp(formatTMS(AppDate, AppTime));
	datetmp.erase(std::remove(datetmp.begin(), datetmp.end(), ':'), datetmp.end());
	datetmp.erase(std::remove(datetmp.begin(), datetmp.end(), '-'), datetmp.end());
	datetmp.erase(std::remove(datetmp.begin(), datetmp.end(), 'T'), datetmp.end());
	bzId.append(datetmp);
	if (isvaluation)
	{
		bzId.append("VALO");
	}
	else
	{
		bzId.append("TRD");
		bzId.append(tradeid);
		bzId.append("V");
		string version = std::to_string(tradeversion);
		bzId.append(version);
	}

	return bzId;
}

string EmirReport::getRootNodeName(const string &actiontype, const string &action)
{
	string rootname;

	if ((actiontype == "N") || (action == "ISNEW") || (action == "ISNEWTORESEND"))
	{ // Document/DerivsTradRpt/TradData/Rpt/New
		rootname = "New";
	}
	else if ((actiontype == "M") || (action == "ISMODI") || (action == "ISMODITORESEND"))
	{ // Document/DerivsTradRpt/TradData/Rpt/Mod
		rootname = "Mod";
	}
	else if ((actiontype == "V") || (action == "ISVALUATION"))
	{ // Document/DerivsTradRpt/TradData/Rpt/ValtnUpd
		rootname = "ValtnUpd";
	}
	else if (((actiontype == "E") || (actiontype == "C")) || (action == "ISCANCEL") || (action == "ISCANCELTORESEND"))
	{ // Document/DerivsTradRpt/TradData/Rpt/Err
		rootname = "Err";
	}
	else
	{ // Document/DerivsTradRpt/TradData/Rpt/Crrctn
		// Document/DerivsTradRpt/TradData/Rpt/PosCmpnt
		// Document/DerivsTradRpt/TradData/Rpt/Cmprssn
		// Document/DerivsTradRpt/TradData/Rpt/Rvv
		// Document/DerivsTradRpt/TradData/Rpt/Othr
		// Document/DerivsTradRpt/TradData/Rpt/Termntn
		rootname = "";
	}

	return rootname;
}

bool EmirReport::updateDB(ChROME::EntityWrapper<cEMRTRACT> &cEMRTRACT2update, const cEMRTRACT *emirTrdActivity, const string &action, const string &bzId, int tradeversion, bool istransferuti)
{
	/*bool foundinDB = getcEMRTRACT(cEMRTRACT2update, emirTrdActivity->UTI.Text, emirTrdActivity->Counterparty1.Text, emirTrdActivity->Counterparty2.Text);
	if (!foundinDB) {
		return false;
	}*/

	string confirmationtimestamp;
	string valuationtimestamp;
	if (action == "ISNEWTORESEND" || action == "ISMODITORESEND")
	{
		confirmationtimestamp = cEMRTRACT2update->ConfirmationTimestamp.Text;
		valuationtimestamp = cEMRTRACT2update->ValuationTimeStamp.Text;
	}

	if ((action != "ISNEW") && (action != "ISERR"))
	{
		if (((action != "ISMODITORESEND") && ((action == "ISMODI") && !istransferuti) || (action == "ISVALUATION")) && !IsDiff(cEMRTRACT2update, emirTrdActivity))
		{
			return false;
		}
		CopyDiff(cEMRTRACT2update, emirTrdActivity);
		// strcpy(cEMRTRACT2update->ReturnStatus.Text, "");
		// memset(&cEMRTRACT2update->ReturnDescription.data, 0, sizeof(cEMRTRACT2update->ReturnDescription.size));
	}

	if (action == "ISNEW")
	{
		strcpy(cEMRTRACT2update->ActionType.Text, "New");
	}
	else if (action == "ISNEWTORESEND")
	{
		strcpy(cEMRTRACT2update->ExecutionTimestamp.Text, emirTrdActivity->ExecutionTimestamp.Text);
		strcpy(cEMRTRACT2update->ConfirmationTimestamp.Text, confirmationtimestamp.c_str());
		strcpy(cEMRTRACT2update->ValuationTimeStamp.Text, valuationtimestamp.c_str());
		strcpy(cEMRTRACT2update->ActionType.Text, "New");
	}
	else if (action == "ISMODI" || action == "ISMODITORESEND")
	{
		strcpy(cEMRTRACT2update->ActionType.Text, "Mod");
	}
	else if (action == "ISCANCEL" || action == "ISCANCELTORESEND")
	{
		strcpy(cEMRTRACT2update->ActionType.Text, "Err");
		strcpy(cEMRTRACT2update->EventType.Text, " "); // on le force comme a cause des tests hist
	}
	else if (action == "ISVALUATION")
	{
		strcpy(cEMRTRACT2update->ActionType.Text, "ValtnUpd");
		strcpy(cEMRTRACT2update->EventType.Text, " ");
		strcpy(cEMRTRACT2update->ExecutionTimestamp.Text, emirTrdActivity->ExecutionTimestamp.Text);
	}

	strcpy(cEMRTRACT2update->ProcessDate.Text, formatDAT(AppDate).c_str());
	strcpy(cEMRTRACT2update->BizMsgIdr.Text, bzId.c_str());
	cEMRTRACT2update->TradeVersion = tradeversion;
	cEMRTRACT2update->FulfillStatus = cEMIRBS_SENT;
	strcpy(cEMRTRACT2update->ReturnStatus.Text, "");
	memset(&cEMRTRACT2update->ReturnDescription.data, 0, sizeof(cEMRTRACT2update->ReturnDescription.size));
	try
	{
		cEMRTRACT2update.doStdAction(sACT_SAVE);
		return true;
	}
	catch (...)
	{
		sLogMessage("GenerateReport::an error occured when updating declaration in table", sLOG_INFO, 0);
		return false;
	}
	return false; // return true;
}

void EmirReport::fillReportExecutionTimestamp(xml_node &rptnode, const string &executiontimestamp)
{ //!!!a faire qu une seule fonction avec xpath
	xml_node exctntmstmpnode = rptnode.select_single_node("root/CmonTradData/TxData/ExctnTmStmp").node();
	exctntmstmpnode.text().set(executiontimestamp.c_str());
}

void EmirReport::fillReportConfirmationTimestamp(xml_node &rptnode, const string &confirmationtimestamp)
{ //!!!a faire qu une seule fonction avec xpath
	xml_node conftmstmpnode = rptnode.select_single_node("root/CmonTradData/TxData/TradConf/Confd/TmStmp").node();
	conftmstmpnode.text().set(confirmationtimestamp.c_str());
}

void EmirReport::fillReport(int nbreport, xml_node &documentnode, const string &executionagentrcp, const string &bzId, const string &tradeid)
{
	xpath_node rpthdrnode = documentnode.select_single_node("DerivsTradRpt/RptHdr");
	rpthdrnode.node().child("NbRcrds").text().set(nbreport);
	removenode(documentnode);

	xml_document headerdoc;
	string srefit2modelHeaderpath = getenv("STKROOT");
	srefit2modelHeaderpath.append("/etc/EMIR_XMLModelFiles/refit2modeleHeader.xml");
	xml_parse_result resultheader = headerdoc.load_file(srefit2modelHeaderpath.c_str());
	xml_node headernode = headerdoc.child("DTCC_Wrapper");
	headernode.child("ExecutionAgentRCP").text().set(executionagentrcp.c_str());
	xml_node pyldnode = headernode.first_element_by_path("BizData").child("Pyld");
	pyldnode.append_copy(documentnode);
	removenode(pyldnode);
	xml_node hdrnode = headernode.first_element_by_path("BizData").child("Hdr");
	hdrnode.select_single_node("AppHdr/Fr/OrgId/Id/OrgId/Othr").node().child("Id").text().set(OCode.c_str());
	// string processdate = formatTMS(AppDate, AppTime) + "Z";
	string processdate = ChROME::UTCTimeStamp(AppDate, AppTime, true);
	hdrnode.select_single_node("AppHdr").node().child("CreDt").text().set(processdate.c_str());
	xml_node bizMsgIdrnode = hdrnode.select_single_node("AppHdr/BizMsgIdr").node();
	bizMsgIdrnode.text().set(bzId.c_str());
	removenode(headernode);
	std::size_t pos = OutputFileName.find(".xml");
	string filename(OutputFileName.substr(0, pos));
	filename.append(tradeid);
	filename.append(".xml");
	// headerdoc.save_file(OutputFileName.c_str());
	headerdoc.save_file(filename.c_str());
}

bool EmirReport::manageFXSWAPNear(xml_node &trddatanode, const sTRADE &trd, const string &bzId, const string &actionfar, bool managehist)
{
	if (trd.FxTrade.Forex->ShortValDate < Date)
	{ // patte echue
		return false;
	}

	EmirFXSwapNear *emirTradeFXSwapNear = new EmirFXSwapNear(&trd, Date, AppDate, Time, AppTime, Debug);
	emirTradeFXSwapNear->Init();
	emirTradeFXSwapNear->SetTransactionType("Trade");

	/*if (isLegTerminated) {
		return false;
	}*/

	string action;
	ChROME::EntityWrapper<cEMRTRACT> emirTrdActivityHistnear("cEMRTRACT");
	try
	{
		strcpy(emirTrdActivityHistnear->Counterparty1.Text, emirTradeFXSwapNear->GetTradeParty1Value().c_str());
		strcpy(emirTrdActivityHistnear->Counterparty2.Text, emirTradeFXSwapNear->GetTradeParty2Value().c_str());
		strcpy(emirTrdActivityHistnear->UTI.Text, emirTradeFXSwapNear->GetUTI().c_str());
		bool found = emirTrdActivityHistnear.sqlRead("EQUAL", 00, true);

		string tradeid = trd.Base.TradeID.Num;
		if (found && tradeid.compare(emirTrdActivityHistnear->TradeID.Text))
		{ // les contreparties + uti sont les mmes pour un tradeid diff => cas d echange d uti
			return false;
		}

		bool isnew = !isValuation && !found;
		bool iscancel = (actionfar == "ISCANCEL") ? true : false;
		bool iscancel2resend = found && !strcmp(emirTrdActivityHistnear->ActionType.Text, "Err") && ((ChROME::trim(emirTrdActivityHistnear->ReturnStatus.Text) == "") || !strcmp(emirTrdActivityHistnear->ReturnStatus.Text, "RJCT"));
		bool isnew2resend = !isValuation && found && !iscancel && !strcmp(emirTrdActivityHistnear->ActionType.Text, "New") && ((ChROME::trim(emirTrdActivityHistnear->ReturnStatus.Text) == "") || !strcmp(emirTrdActivityHistnear->ReturnStatus.Text, "RJCT"));
		bool ismodi = !isValuation && found && !iscancel && strcmp(emirTrdActivityHistnear->ActionType.Text, "Err") && !strcmp(emirTrdActivityHistnear->ReturnStatus.Text, "ACPT") && (!strcmp(emirTrdActivityHistnear->ActionType.Text, "New") || !strcmp(emirTrdActivityHistnear->ActionType.Text, "Mod"));
		bool ismodi2resend = !isValuation && found && !iscancel && !strcmp(emirTrdActivityHistnear->ActionType.Text, "Mod") && ((ChROME::trim(emirTrdActivityHistnear->ReturnStatus.Text) == "") || !strcmp(emirTrdActivityHistnear->ReturnStatus.Text, "RJCT"));

		if (actionfar == "ISCANCEL")
		{
			action = "ISCANCEL";
		}
		else if (iscancel2resend)
		{
			action = "ISCANCELTORESEND";
		}
		else if (isnew)
		{
			action = "ISNEW";
		}
		else if (isnew2resend)
		{
			action = "ISNEWTORESEND";
		}
		else if (ismodi)
		{
			action = "ISMODI";
		}
		else if (ismodi2resend)
		{
			action = "ISMODITORESEND";
		}
		else if (isValuation)
		{
			action = "ISVALUATION";
		}
		else
		{
			return false;
		}
	}
	catch (...)
	{
		action = "ISNEW";
	}

	stringstream resultxmlnear;
	ChROME::EntityWrapper<cEMRTRACT> emirTrdActivitynear("cEMRTRACT");
	if (action == "ISNEW")
	{
		emirTradeFXSwapNear->SetActionTypeParty1(cACTTYP_NEW);
		strcpy(emirTrdActivitynear->ActionType.Text, "New");
		strcpy(emirTrdActivitynear->TradeID.Text, trd.Base.TradeID.Num);
	}
	else
	{
		if (action == "ISNEWTORESEND")
		{
			emirTradeFXSwapNear->SetActionTypeParty1(cACTTYP_NEW);
			strcpy(emirTrdActivitynear->ActionType.Text, "New");
		}
		else if (action == "ISMODI" || action == "ISMODITORESEND")
		{
			emirTradeFXSwapNear->SetActionTypeParty1(cACTTYP_MODIFY);
			strcpy(emirTrdActivitynear->ActionType.Text, "Mod");
		}
		else if (action == "ISCANCEL" || action == "ISCANCELTORESEND")
		{
			emirTradeFXSwapNear->SetActionTypeParty1(cACTTYP_CANCEL);
			strcpy(emirTrdActivitynear->ActionType.Text, "Err");
		}
		else if (action == "ISVALUATION")
		{
			emirTradeFXSwapNear->SetActionTypeParty1(cACTTYP_VALUATION);
			strcpy(emirTrdActivitynear->ActionType.Text, "ValtnUpd");
		}
		else
		{
			return false;
		}
	}
	emirTradeFXSwapNear->PrintXML(*emirTrdActivitynear, &resultxmlnear, managehist);

	bool updb = false;
	if (action == "ISNEW")
	{
		updb = updateDB(emirTrdActivitynear, emirTrdActivitynear, action, bzId, trd.Base.Env->Audit.Version);
	}
	else
	{
		strcpy(emirTrdActivityHistnear->ActionType.Text, emirTrdActivitynear->ActionType.Text);
		updb = updateDB(emirTrdActivityHistnear, emirTrdActivitynear, action, bzId, trd.Base.Env->Audit.Version);
	}
	if (!updb)
	{
		return updb;
	}

	xml_document resultdocnear;
	xml_parse_result r = resultdocnear.load(resultxmlnear);
	xml_node resultnode = resultdocnear.child("root");
	// string rootname = emirTrdActivitynear->ActionType.Text;
	string rootname = getRootNodeName("", action);
	resultnode.set_name(rootname.c_str());
	trddatanode.append_child("Rpt").append_copy(resultnode);

	if (!isValuation)
	{
		manageExitUTI(trddatanode, emirTradeFXSwapNear);
	}

	return true;
}

/*if (sEntityDBRead(sGetEntityByName("cEMRTRACT"), (void*)emirtractHist, "EQUAL", 00) != sSUCCESS) {
		//EXIT
		// New UTI
		ChROME::EntityWrapper<cEMRTRACT> emirSameTrade("cEMRTRACT");
		ostringstream sqlQueryTrade;
		sqlQueryTrade << "where TradeId = '" << emirTrade->GetTradeId() << "' "
			<< "and contracttype= '"<< emirTrade->GetTradeType() <<"' "
			<< "and counterparty1 = '" << emirTrade->GetTradeParty1Value() << "' "
			<< "and counterparty2 = '" << emirTrade->GetTradeParty2Value() << "' "
			<< "and UTI != '" << emirTrade->GetUTI() << "' "
			<< "and not (actiontype = 'Err' and returnstatus = 'RJCT' and returnstatus = 'ACKN' and returnstatus = 'NAUT') "
			<< "and Audit_Current = 'Y' ";

		while ( emirSameTrade.sqlRead(sqlQueryTrade.str(), 00, false) ) {
			sInsertListItem(&EmirExitTradesList, EmirExitTradesList.List.ItemsUsed, emirSameTrade);
			sLogMessage("EmirReport::IsEligibleForDeclaration New UTI - TradeList tradeid = %s emirtract tradeid = %s",  sLOG_INFO, 0, trade->Base.TradeID.Num, emirSameTrade->TradeID.Text);
		}
		emirSameTrade.sqlCancel();

		//EXIT
		// Existing UTI
		ChROME::EntityWrapper<cEMRTRACT> emirSameUTI("cEMRTRACT");
		ostringstream sqlQueryUTI;
		sqlQueryUTI << "where UTI = '" << emirTrade->GetUTI() << "' "
			<< "and counterparty1 = '" << emirTrade->GetTradeParty1Value() << "' "
			<< "and ( counterparty2 != '" << emirTrade->GetTradeParty2Value()  << "') "
			<< "and not (actiontype = 'Err' and returnstatus = 'RJCT' and returnstatus = 'ACKN' and returnstatus = 'NAUT') "
			<< "and Audit_Current = 'Y' ";

		while ( emirSameUTI.sqlRead(sqlQueryUTI.str(), 00, false) ) {
			sInsertListItem(&EmirExitTradesList, EmirExitTradesList.List.ItemsUsed, emirSameUTI);
			sLogMessage("EmirReport::IsEligibleForDeclaration Existing UTI - TradeList tradeid = %s emirtract tradeid = %s",  sLOG_INFO, 0, trade->Base.TradeID.Num,emirSameUTI->TradeID.Text);
		}
		emirSameUTI.sqlCancel();

		//EXIT
		//New UTI and new LEI
		ChROME::EntityWrapper<cEMRTRACT> emirNewUTILEI("cEMRTRACT");
		ostringstream sqlQueryNewUTILEI;
		sqlQueryNewUTILEI << "where TradeId = '" << emirTrade->GetTradeId() << "' "
			<< "and contracttype='"<< emirTrade->GetTradeType() <<"' "
			<< "and UTI != '" << emirTrade->GetUTI() << "' "
			<< "and counterparty1 = '" << emirTrade->GetTradeParty1Value() << "' "
			<< "and ( counterparty2 != '" << emirTrade->GetTradeParty2Value()  << "') "
			<< "and not (actiontype = 'Err' and returnstatus = 'RJCT' and returnstatus = 'ACKN' and returnstatus = 'NAUT') "
			<< "and Audit_Current = 'Y' ";

		while ( emirNewUTILEI.sqlRead(sqlQueryNewUTILEI.str(), 00, false) ) {
			sInsertListItem(&EmirExitTradesList, EmirExitTradesList.List.ItemsUsed, emirNewUTILEI);
			sLogMessage("EmirReport::IsEligibleForDeclaration NEWUTI NEWLEI - TradeList tradeid = %s emirtract tradeid = %s",  sLOG_INFO, 0, trade->Base.TradeID.Num,emirNewUTILEI->TradeID.Text);
		}
		emirNewUTILEI.sqlCancel();

		// New declaration to send
		emirTrade->SetActionTypeParty1(cACTTYP_NEW);
		emirTrade->SetTransactionType("Trade");

		if (emirTrade->GetDelegation() == sYES) {
			emirTrade->SetActionTypeParty2(cACTTYP_NEW);
		}

		return true;
	}*/
bool EmirReport::manageExitUTI(xml_node &trddatanode, EmirTrade *emirTrade)
{
	ChROME::EntityWrapper<cEMRTRACT> emirTrdActivity2exithist("cEMRTRACT");
	ostringstream sqlQueryTrade;
	sqlQueryTrade << "where TradeId = '" << emirTrade->GetTradeId() << "' "
				  //<< "and contracttype = '"<< emirTrade->GetTradeType() <<"' "
				  << "and counterparty1 = '" << emirTrade->GetTradeParty1Value() << "' "
				  << "and counterparty2 = '" << emirTrade->GetTradeParty2Value() << "' "
				  << "and UTI != '" << emirTrade->GetUTI() << "' "
				  /*<< "and actiontype != 'Err'"
				  << "and returnstatus = 'ACPT' "
				  << "and tradeid not in (select tradeid from cdmemirtrdactivity_hist where actiontype='Err' and returnstatus= 'ACPT')"*/
				  << "and Audit_Current = 'Y' "
				  << "order by audit_datetime desc";

	try
	{
		bool found = emirTrdActivity2exithist.sqlRead(sqlQueryTrade.str(), 00, true);
		if (!found)
		{
			return false;
		}
		ChROME::EntityWrapper<cEMRTRACT> cEMRTRACTErrAcpt("cEMRTRACT");
		bool founderracpt = getNewModiAcptNoErr(cEMRTRACTErrAcpt, emirTrdActivity2exithist->UTI.Text, emirTrdActivity2exithist->Counterparty1.Text, emirTrdActivity2exithist->Counterparty2.Text);
		if (!founderracpt)
		{
			return false;
		} // sinon on traite en dessous
		if (!strcmp(emirTrdActivity2exithist->ActionType.Text, "ValtnUpd"))
		{
			ChROME::EntityWrapper<cEMRTRACT> cEMRTRACTnonValtnUpd("cEMRTRACT");
			bool foundnonvalupd = getcEMRTRACTNoValtnUpd(cEMRTRACTnonValtnUpd, emirTrdActivity2exithist->UTI.Text, emirTrdActivity2exithist->Counterparty1.Text, emirTrdActivity2exithist->Counterparty2.Text);
			if (foundnonvalupd)
			{
				emirTrdActivity2exithist.copyWithoutAudit(cEMRTRACTnonValtnUpd); // Ne pas oublier de sauvegarder sur la derniere version en cours !!champs d audit
			}
			found = foundnonvalupd;
		}
	}
	catch (...)
	{
		return false;
	}

	/*if (!strcmp(emirTrdActivity2exithist->UTI.Text, emirTrdActivity->UTI.Text) {
		xml_node resultnode = documentnode.child("DerivsTradRpt/TradData/Rpt/root/CmonTradData/TxData/DerivEvt/Tp");
		resultnode.text().set("NOVA");
	}*/

	stringstream resultxmlerr;
	printErrXML(emirTrdActivity2exithist, &resultxmlerr);

	bool updb = updateDB(emirTrdActivity2exithist, emirTrdActivity2exithist, "ISERR", emirTrdActivity2exithist->BizMsgIdr.Text, emirTrdActivity2exithist->TradeVersion);
	if (!updb)
	{
		return updb;
	}

	xml_document resultdocerr;
	xml_parse_result r = resultdocerr.load(resultxmlerr);
	xml_node errnode = resultdocerr.child("Err");
	trddatanode.append_child("Rpt").append_copy(errnode);

	return true;
}

bool EmirReport::getcEMRTRACT(ChROME::EntityWrapper<cEMRTRACT> &cEMRTRACT2read, bool &istransferuti, const string &uti, const string &counterparty1, const string &counterparty2, const string &tradeid)
{
	istransferuti = false;

	strcpy(cEMRTRACT2read->UTI.Text, uti.c_str());
	strcpy(cEMRTRACT2read->Counterparty1.Text, counterparty1.c_str());
	strcpy(cEMRTRACT2read->Counterparty2.Text, counterparty2.c_str());
	try
	{
		bool found = cEMRTRACT2read.sqlRead("EQUAL", 00, true);
		if (found && !strcmp(cEMRTRACT2read->ActionType.Text, "ValtnUpd"))
		{
			ChROME::EntityWrapper<cEMRTRACT> cEMRTRACTnonValtnUpd("cEMRTRACT");
			bool foundnonvalupd = getcEMRTRACTNoValtnUpd(cEMRTRACTnonValtnUpd, uti, counterparty1, counterparty2);
			if (foundnonvalupd)
			{
				cEMRTRACT2read.copyWithoutAudit(cEMRTRACTnonValtnUpd); // Ne pas oublier de sauvegarder sur la derniere version en cours !!champs d audit
																	   // CopyDiff(cEMRTRACT2read, cEMRTRACTnonValtnUpd);
			}
			found = foundnonvalupd;
		}
		if (found && tradeid.compare(cEMRTRACT2read->TradeID.Text))
		{						  // les contreparties + uti sont les mmes pour un tradeid diff => cas d echange d uti
			istransferuti = true; // comment ne pas forcer un uti sur un trade non cancel => verifier que l autre trade est en cancel
			strcpy(cEMRTRACT2read->TradeID.Text, tradeid.c_str());
			// return false;
		}
		return found;
	}
	catch (...)
	{
		sLogMessage("EmirReport::getcEMRTRACT::no declaration in DB with counterparty1(%s) counterparty2(%s) uti(%s)", sLOG_INFO, 0, counterparty1.c_str(), counterparty2.c_str(), uti.c_str());
		return false;
	}

	return false;
}

bool EmirReport::getcEMRTRACTNoValtnUpd(ChROME::EntityWrapper<cEMRTRACT> &cEMRTRACT2read, const string &uti, const string &counterparty1, const string &counterparty2, const string &bzid)
{
	ostringstream sqlQueryTrade;
	sqlQueryTrade << "where counterparty1 = '" << counterparty1.c_str() << "' and counterparty2 = '" << counterparty2.c_str() << "' and UTI = '" << uti.c_str() << "' and actiontype != 'ValtnUpd' and audit_current='Y' order by audit_version desc";
	try
	{
		string sql = sqlQueryTrade.str();
		bool found = cEMRTRACT2read.sqlRead(sqlQueryTrade.str(), sHISTORY_REC | sRM_RAW_MODE);
		return found;
	}
	catch (...)
	{
		sLogMessage("EmirReport::getcEMRTRACTNoValtnUpd::no declaration in DB with counterparty1(%s) counterparty2(%s) uti(%s)", sLOG_INFO, 0, counterparty1.c_str(), counterparty2.c_str(), uti.c_str());
		return false;
	}

	return false;
}

bool EmirReport::getcEMRTRACTNoErrAcpt(ChROME::EntityWrapper<cEMRTRACT> &cEMRTRACT2read, const string &uti, const string &counterparty1, const string &counterparty2, const string &bzid)
{
	ostringstream sqlQueryTrade;
	sqlQueryTrade << "where counterparty1 = '" << counterparty1.c_str() << "' and counterparty2 = '" << counterparty2.c_str() << "' and UTI = '" << uti.c_str() << "' and actiontype = 'Err' and returnstatus= 'ACPT'  order by audit_version desc";
	sLogMessage("EmirReport::getcEMRTRACTNoErrAcpt::sqlQueryTrade.str()(%s)", sLOG_INFO, 0, sqlQueryTrade.str());
	try
	{
		string sql = sqlQueryTrade.str();
		bool found = cEMRTRACT2read.sqlRead(sqlQueryTrade.str(), sHISTORY_REC | sRM_RAW_MODE);
		return found;
	}
	catch (...)
	{
		sLogMessage("EmirReport::getcEMRTRACTNoErrAcpt::no declaration in DB with counterparty1(%s) counterparty2(%s) uti(%s) Err Acpt", sLOG_INFO, 0, counterparty1.c_str(), counterparty2.c_str(), uti.c_str());
		return false;
	}

	return false;
}

bool EmirReport::getNewModiAcptNoErr(ChROME::EntityWrapper<cEMRTRACT> &cEMRTRACT2read, const string &uti, const string &counterparty1, const string &counterparty2, const string &bzid)
{
	ostringstream sqlQueryTrade;
	sqlQueryTrade << "where counterparty1 = '" << counterparty1.c_str() << "' and counterparty2 = '" << counterparty2.c_str() << "' and UTI = '" << uti.c_str() << "' and returnstatus= 'ACPT' order by audit_version desc";
	sLogMessage("EmirReport::getNewModiAcptNoErr::sqlQueryTrade.str()(%s)", sLOG_INFO, 0, sqlQueryTrade.str());
	try
	{
		string sql = sqlQueryTrade.str();
		bool found = cEMRTRACT2read.sqlRead(sqlQueryTrade.str(), sHISTORY_REC | sRM_RAW_MODE);
		if (found && !strcmp(cEMRTRACT2read->ActionType.Text, "Err"))
		{
			return false;
		}
		return found;
	}
	catch (...)
	{
		sLogMessage("EmirReport::getNewModiAcptNoErr::no declaration in DB with counterparty1(%s) counterparty2(%s) uti(%s) Acpt", sLOG_INFO, 0, counterparty1.c_str(), counterparty2.c_str(), uti.c_str());
		return false;
	}

	return false;
}

bool EmirReport::getcEMRTRACTSameTradeIDOtherUTI(ChROME::EntityWrapper<cEMRTRACT> &cEMRTRACT2read, const string &uti, const string &counterparty1, const string &counterparty2, const string &tradeid)
{
	ChROME::EntityWrapper<cEMRTRACT> emirTrdActivity2exithist("cEMRTRACT");
	ostringstream sqlQueryTrade;
	sqlQueryTrade << "where TradeId = '" << tradeid.c_str() << "' and counterparty1 = '" << counterparty1.c_str() << "' and counterparty2 = '" << counterparty2.c_str() << "' and UTI != '" << uti.c_str() << "' and Audit_Current = 'Y' order by audit_datetime desc";
	sLogMessage("EmirReport::getcEMRTRACTSameTradeIDOtherUTI::sqlQueryTrade.str()(%s)", sLOG_INFO, 0, sqlQueryTrade.str());
	try
	{
		string sql = sqlQueryTrade.str();
		bool found = cEMRTRACT2read.sqlRead(sqlQueryTrade.str(), sHISTORY_REC | sRM_RAW_MODE);
		if (found && !strcmp(cEMRTRACT2read->ActionType.Text, "Err"))
		{
			return false;
		}
		return found;
	}
	catch (...)
	{
		sLogMessage("EmirReport::getcEMRTRACTSameTradeIDOtherUTI::no declaration in DB with counterparty1(%s) counterparty2(%s) TradeId(%s) Acpt", sLOG_INFO, 0, counterparty1.c_str(), counterparty2.c_str(), uti.c_str());
		return false;
	}

	return false;
}

/*bool EmirReport::getACPTwithoutReturn(ChROME::EntityWrapper<cEMRTRACT>& cEMRTRACT2read, const string& uti, const string& counterparty1, const string& counterparty2, const string& tradeid) {
	ostringstream sqlQueryTrade;
	sqlQueryTrade << "where counterparty1 = '" << counterparty1.c_str() << "' and counterparty2 = '" << counterparty2.c_str() << "' and UTI = '" << uti.c_str() << "' and returnstatus = 'RJCT' and returndescription like '%EMIR-VR-2151-03%' and ROWNUM = 1";
	try {
		string sql = sqlQueryTrade.str();
		bool found = cEMRTRACT2read.sqlRead(sqlQueryTrade.str(), sHISTORY_REC | sRM_RAW_MODE | sRM_LATEST_MODE);
		return found;
	} catch (...) {
		sLogMessage("EmirReport::getcEMRTRACT::no declaration in DB with counterparty1(%s) counterparty2(%s) uti(%s) status EMIR-VR-2151-03", sLOG_INFO, 0, counterparty1.c_str(), counterparty2.c_str(), uti.c_str());
		return false;
	}

	return false;
}*/

void EmirReport::GenerateReport()
{
	try
	{
		// Load trades in the filter
		LoadTrades();

		// Filter duplicate UTI (replace the UTI with "DUPLICATEUTI" for all except the trades we will report)
		FilterDuplicateUTI();

		// Pour bloquer la base

		if (sSUCCESS != sDBBeginTran())
		{
			string ErrMsg = "Error starting Database Transaction";
			throw SummitFatalException(ErrMsg);
		}

		string srefit2modelDocumentpath = getenv("STKROOT");
		srefit2modelDocumentpath.append("/etc/EMIR_XMLModelFiles/refit2modeleDocument.xml");
		std::ifstream modeldocfile(srefit2modelDocumentpath);
		std::stringstream modeldocinstream;
		modeldocinstream << modeldocfile.rdbuf();

		// valuation
		int nbreportval = 0;
		xml_document docval;
		xml_parse_result resultval = docval.load_file(srefit2modelDocumentpath.c_str());
		xml_node documentnodeval = docval.child("Document");
		string bzId;
		if (isValuation)
		{
			computeBZID(bzId, "", 0, isValuation, isHIST);
		}

		for (SummitList<sTRADELIST, sTRADE>::const_iterator itTrd = (*TradeList).begin(); itTrd != (*TradeList).end(); itTrd++)
		{
			int nbreport = 0;
			xml_document doc;
			xml_parse_result result = doc.load_string(modeldocinstream.str().c_str());
			xml_node documentnode = doc.child("Document");
			if (!isValuation)
			{
				computeBZID(bzId, itTrd->Base.TradeID.Num, itTrd->Base.Env->Audit.Version, isValuation, isHIST);
			}

			try
			{
				sLogMessage("Generating report for tradeid: %s", sLOG_ERROR, 0, itTrd->Base.TradeID.Num);

				if (IsExcluded(&(*itTrd)) == true)
				{
					continue;
				}

				bool istrdcancelstate = (itTrd->Base.Env->Audit.StdAction == sACT_CANCEL || itTrd->Base.Env->Audit.StdAction == cACT_RETA || itTrd->Base.Env->Audit.StdAction == sACT_ACCEPT) && (itTrd->Base.Env->Audit.EntityState == sTS_CANCELLED && itTrd->Base.Env->Audit.Authorized == sYES);
				if (!strcmp(itTrd->Base.Back->cUniqueTradeId.Text, "DUPLICATEUTI") && istrdcancelstate)
				{
					continue;
				}

				EmirTrade *emirTrade = InitEmirTrade(&(*itTrd));
				if (emirTrade == NULL)
				{
					continue;
				}

				emirTrade->Init();
				emirTrade->SetTransactionType("Trade");
				emirTrade->NDALARMTIME = NDALARMTIME; // mis en place pour gerer l historique des declarations a modifier pour mettre un getter plutot que plublic
				sLogMessage("GenerateReport::NDALARMTIME(%s), emirTrade->NDALARMTIME(%s)", sLOG_INFO, 0, ChROME::sIDATEtoString(NDALARMTIME), ChROME::sIDATEtoString(emirTrade->NDALARMTIME));

				ChROME::EntityWrapper<cEMRTRACT> emirTrdActivity("cEMRTRACT");
				// strcpy(emirTrdActivity->TradeID.Text, emirTrade->GetTradeId().c_str());
				strcpy(emirTrdActivity->TradeID.Text, itTrd->Base.TradeID.Num);
				ChROME::EntityWrapper<cEMRTRACT> emirTrdActivityHist("cEMRTRACT");

				bool istransferuti = false;
				bool foundinDB = getcEMRTRACT(emirTrdActivityHist, istransferuti, emirTrade->GetUTI().c_str(), emirTrade->GetTradeParty1Value().c_str(), emirTrade->GetTradeParty2Value().c_str(), itTrd->Base.TradeID.Num);

				if (foundinDB)
				{
					ChROME::EntityWrapper<cEMRTRACT> cEMRTRACTErrAcpt("cEMRTRACT");
					bool founderracpt = getcEMRTRACTNoErrAcpt(cEMRTRACTErrAcpt, emirTrade->GetUTI().c_str(), emirTrade->GetTradeParty1Value().c_str(), emirTrade->GetTradeParty2Value().c_str());
					if (founderracpt)
					{
						continue;
					}
				}

				bool iscancel = !isValuation && foundinDB && istrdcancelstate;
				bool iscancel2resend = !strcmp(emirTrdActivityHist->ActionType.Text, "Err") && ((ChROME::trim(emirTrdActivityHist->ReturnStatus.Text) == "") || !strcmp(emirTrdActivityHist->ReturnStatus.Text, "RJCT"));
				bool isnew = !isValuation && !foundinDB;
				bool isnew2resend = !isValuation && foundinDB && !iscancel && !strcmp(emirTrdActivityHist->ActionType.Text, "New") && ((ChROME::trim(emirTrdActivityHist->ReturnStatus.Text) == "") || !strcmp(emirTrdActivityHist->ReturnStatus.Text, "RJCT"));
				bool ismodi = !isValuation && foundinDB && !iscancel && strcmp(emirTrdActivityHist->ActionType.Text, "Err") && !strcmp(emirTrdActivityHist->ReturnStatus.Text, "ACPT") && (!strcmp(emirTrdActivityHist->ActionType.Text, "New") || !strcmp(emirTrdActivityHist->ActionType.Text, "Mod"));
				bool ismodi2resend = !isValuation && foundinDB && !iscancel && !strcmp(emirTrdActivityHist->ActionType.Text, "Mod") && ((ChROME::trim(emirTrdActivityHist->ReturnStatus.Text) == "") || !strcmp(emirTrdActivityHist->ReturnStatus.Text, "RJCT"));

				string action;
				if (isValuation)
				{
					if (!(foundinDB && !iscancel && strcmp(emirTrdActivityHist->ActionType.Text, "Err") && (!strcmp(emirTrdActivityHist->ReturnStatus.Text, "ACPT") || !strcmp(emirTrdActivityHist->ActionType.Text, "Mod"))))
					{
						continue;
					}
					if (foundinDB && !strcmp(emirTrdActivityHist->ActionType.Text, "Mod") && !strcmp(emirTrdActivityHist->EventType.Text, "ETRM") && !strcmp(emirTrdActivityHist->ReturnStatus.Text, "ACPT"))
					{
						continue;
					}
					emirTrade->SetActionTypeParty1(cACTTYP_VALUATION);
					if (emirTrade->GetDelegation() == sYES)
					{
						emirTrade->SetActionTypeParty2(cACTTYP_VALUATION);
					}
					action = "ISVALUATION";
				}
				else if (isnew)
				{
					emirTrade->SetActionTypeParty1(cACTTYP_NEW);
					if (emirTrade->GetDelegation() == sYES)
					{
						emirTrade->SetActionTypeParty2(cACTTYP_NEW);
					}
					action = "ISNEW";
				}
				else if (isnew2resend)
				{
					emirTrade->SetActionTypeParty1(cACTTYP_NEW);
					if (emirTrade->GetDelegation() == sYES)
					{
						emirTrade->SetActionTypeParty2(cACTTYP_NEW);
					}
					action = "ISNEWTORESEND";
				}
				else if (ismodi)
				{
					emirTrade->SetActionTypeParty1(cACTTYP_MODIFY);
					if (emirTrade->GetDelegation() == sYES)
					{
						emirTrade->SetActionTypeParty2(cACTTYP_MODIFY);
					}
					action = "ISMODI";
				}
				else if (ismodi2resend)
				{
					emirTrade->SetActionTypeParty1(cACTTYP_MODIFY);
					if (emirTrade->GetDelegation() == sYES)
					{
						emirTrade->SetActionTypeParty2(cACTTYP_MODIFY);
					}
					action = "ISMODITORESEND";
				}
				else if (iscancel)
				{
					emirTrade->SetActionTypeParty1(cACTTYP_CANCEL);
					if (emirTrade->GetDelegation() == sYES)
					{
						emirTrade->SetActionTypeParty2(cACTTYP_CANCEL);
					}
					action = "ISCANCEL";
				}
				else if (iscancel2resend)
				{
					emirTrade->SetActionTypeParty1(cACTTYP_CANCEL);
					if (emirTrade->GetDelegation() == sYES)
					{
						emirTrade->SetActionTypeParty2(cACTTYP_CANCEL);
					}
					action = "ISCANCELTORESEND";
				}
				else
				{
					continue;
				}

				stringstream resultxml;
				emirTrade->PrintXML(*emirTrdActivity, &resultxml);

				if (ismodi && !istransferuti && !IsDiff(emirTrdActivityHist, emirTrdActivity))
				{
					continue;
				}

				xml_document resultdoc;
				xml_parse_result r = resultdoc.load(resultxml);
				if (!r)
				{
					continue;
				}

				if (ismodi || ismodi2resend)
				{
					fillReportExecutionTimestamp(resultdoc, emirTrdActivityHist->ExecutionTimestamp.Text);
					fillReportConfirmationTimestamp(resultdoc, emirTrdActivityHist->ConfirmationTimestamp.Text);
				}

				bool updb = false;
				if (isnew)
				{
					updb = updateDB(emirTrdActivity, emirTrdActivity, action, bzId, itTrd->Base.Env->Audit.Version);
				}
				else
				{
					updb = updateDB(emirTrdActivityHist, emirTrdActivity, action, bzId, itTrd->Base.Env->Audit.Version, istransferuti);
				}
				if (!updb)
				{
					continue;
				}

				xml_node resultnode = resultdoc.child("root");
				// string rootname = emirTrdActivity->ActionType.Text;
				string rootname = getRootNodeName("", action);
				resultnode.set_name(rootname.c_str());

				xml_node trddatanode;
				if (isValuation)
				{
					trddatanode = documentnodeval.first_element_by_path("DerivsTradRpt/TradData");
					++nbreportval;
				}
				else
				{
					trddatanode = documentnode.first_element_by_path("DerivsTradRpt/TradData");
					++nbreport;
				}
				trddatanode.append_child("Rpt").append_copy(resultnode);

				if (!isValuation && !istransferuti && (itTrd->Base.TradeType != sTT_FXSWAP))
				{
					bool exdb = manageExitUTI(trddatanode, emirTrade);
					if (exdb)
					{
						++nbreport;
					}
				}

				bool isfxswapnearfarlegs2declare = (itTrd->Base.TradeType == sTT_FXSWAP) && strlen(itTrd->Base.Back->cUniqueTradeId2.Text) && (!emirTrade->isClkTrd());
				if (isfxswapnearfarlegs2declare)
				{
					string priorutipath = rootname + "/CmonTradData/TxData/TxId/Prtry";
					xml_node priorutinode = resultnode.select_single_node(priorutipath.c_str()).node();
					bool managehist = ChROME::trim(priorutinode.text().get()).empty() ? false : true;
					bool updbnear = manageFXSWAPNear(trddatanode, *itTrd, bzId, action, managehist);
					if (updbnear)
					{
						++nbreportval;
						++nbreport;
					}
				}

				if (!isValuation && nbreport)
				{
					fillReport(nbreport, documentnode, emirTrdActivity->Counterparty1.Text, bzId, itTrd->Base.TradeID.Num);
				}

				delete emirTrade;
			}
			catch (SummitNonFatalException &ex)
			{
				sLogMessage("%s", sLOG_ERROR, 0, ex.what());
				sLogMessage("Error Generating report for tradeid: %s", sLOG_ERROR, 0, itTrd->Base.TradeID.Num);
				continue;
			}
		}

		if (isValuation && nbreportval)
		{
			fillReport(nbreportval, documentnodeval, "969500G1G2X6QKWOD237", bzId, ""); // LEI de BPCESF
		}

		if (sSUCCESS != sDBEndTran())
		{
			string ErrMsg = "Error ending Database Transaction";
			throw SummitFatalException(ErrMsg);
		}
	}
	catch (SummitFatalException &ex)
	{
		// Rollback DB transaction
		sDBRollBack();
		throw SummitFatalException(ex.what());
	}
}

void EmirReport::FilterDuplicateUTI()
{
	// Search to see if we have duplicate UTIs and take the UTI with the gratest maturity
	for (SummitList<sTRADELIST, sTRADE>::const_iterator itTrd1 = (*TradeList).begin(); itTrd1 != (*TradeList).end(); itTrd1++)
	{
		sIDATE matDate = sTradeMatDate((sTRADE *)&(*itTrd1), 00);
		for (SummitList<sTRADELIST, sTRADE>::const_iterator itTrd2 = (*TradeList).begin(); itTrd2 != (*TradeList).end(); itTrd2++)
		{
			if ((strcmp(itTrd1->Base.Back->cUniqueTradeId.Text, itTrd2->Base.Back->cUniqueTradeId.Text) == 0) && (strcmp(itTrd1->Base.TradeID.Num, itTrd2->Base.TradeID.Num) != 0) && ((itTrd1->Base.Env->Audit.EntityState == sTS_CANCELLED) && (itTrd2->Base.Env->Audit.EntityState != sTS_CANCELLED)))
			{
				sLogMessage("Trade: %s canceled and replaced by Trade: %s", sLOG_ERROR, 0, itTrd1->Base.TradeID.Num, itTrd2->Base.TradeID.Num);
				strcpy(itTrd1->Base.Back->cUniqueTradeId.Text, "DUPLICATEUTI");
			}
			if ((strcmp(itTrd1->Base.Back->cUniqueTradeId.Text, itTrd2->Base.Back->cUniqueTradeId.Text) == 0) && (strcmp(itTrd1->Base.TradeID.Num, itTrd2->Base.TradeID.Num) != 0) && ((itTrd1->Base.Env->Audit.EntityState != sTS_CANCELLED) && (itTrd2->Base.Env->Audit.EntityState == sTS_CANCELLED)))
			{
				sLogMessage("Trade: %s canceled and replaced by Trade: %s", sLOG_ERROR, 0, itTrd2->Base.TradeID.Num, itTrd1->Base.TradeID.Num);
				strcpy(itTrd2->Base.Back->cUniqueTradeId.Text, "DUPLICATEUTI");
			}
			else if ((strcmp(itTrd1->Base.Back->cUniqueTradeId.Text, itTrd2->Base.Back->cUniqueTradeId.Text) == 0) && (strcmp(itTrd1->Base.TradeID.Num, itTrd2->Base.TradeID.Num) != 0) && (strcmp(itTrd1->Base.Env->Company.Name, itTrd2->Base.Env->Company.Name) == 0) && (strcmp(itTrd1->Base.Env->Cust.Name, itTrd2->Base.Env->Cust.Name) == 0))
			{
				if (sTradeMatDate((sTRADE *)&(*itTrd2), 00) < matDate)
				{
					sLogMessage("Found two trades with the same UTI Trade 1: %s and Trade 2: %s, Chosen Trade: %s", sLOG_ERROR, 0, itTrd1->Base.TradeID.Num, itTrd2->Base.TradeID.Num, itTrd1->Base.TradeID.Num);
					strcpy(itTrd2->Base.Back->cUniqueTradeId.Text, "DUPLICATEUTI");
				}
				else
				{
					sLogMessage("Found two trades with the same UTI Trade 1: %s and Trade 2: %s, Chosen Trade: %s", sLOG_ERROR, 0, itTrd1->Base.TradeID.Num, itTrd2->Base.TradeID.Num, itTrd2->Base.TradeID.Num);
					strcpy(itTrd1->Base.Back->cUniqueTradeId.Text, "DUPLICATEUTI");
					goto cont;
				}
			}
		}

	cont:;
	}
}

void EmirReport::LoadTrades()
{
	TradeList = auto_ptr<SummitList<sTRADELIST, sTRADE>>(new SummitList<sTRADELIST, sTRADE>("sTRADE"));

	if (!TradeList.get())
	{
		string ErrMsg = "Error initializing Trade List";
		throw SummitFatalException(ErrMsg);
	}

	try
	{
		if (Date != sMAXUS)
		{
			// load last versions according to Date
			debug() << "load last versions according to date " << Date << "  " << strDate(Date) << endl;
			(*TradeList).readTradeListAsOf((*Filter).instance(), Date, 02 | sRM_LATEST_MODE);
		}
		else
		{
			// load last versions according the filter
			//  (*TradeList).readTradeList ( (*Filter).instance(), sRM_LATEST_MODE | 04000 );
			//  (*TradeList).readTradeList ( (*Filter).instance(),004040);
			debug() << "load last versions according the filter " << endl;
			(*TradeList).readTradeList((*Filter).instance(), 00204440); // BZ1797 : remplacement de sRM_LATEST_MODE = 00200000
																		//           04000 recuperer  les bonds expires
																		//           00040 recuperer les futures expires
																		//           00400 recuperer les LOPT expirees
		}
	}
	catch (SummitException &ex)
	{
		throw SummitFatalException(ex.what());
	}
}

bool EmirReport::IsExcluded(const sTRADE *trade)
{
	// Filter duplicate UTI (check if the UTI is "DUPLICATEUTI")
	if (strcmp(trade->Base.Back->cUniqueTradeId.Text, "DUPLICATEUTI") == 0)
	{
		sLogMessage("EmirReport::IsExcluded - Duplicate UTI", sLOG_INFO, 0);
		return true;
	}

	// Operation does not have an UTI
	if (!strlen(trade->Base.Back->cUniqueTradeId.Text))
	{
		sLogMessage("EmirReport::IsExcluded - UTI non renseigne", sLOG_INFO, 0);
		return true;
	}

	// Operation terminated before today
	// default min Full Termination Date to -DATE param value
	sIDATE minFTermDate = Date;
	if (iFTermBKVDays != 0)
	{
		sCAL calName = {"BAT"};
		sCALDATA *cal = sGetCalData(calName);
		// compute in Full Termination Date from BAT calendar
		// and OffSet from Gateway Data Mapping EMIR_GAPS
		// Offset Value from GapType=GAPFTERM
		minFTermDate = sBusDate(Date - iFTermBKVDays, cal, sPREVIOUS);

		char buf[50] = {0};
		string minFTermDateStr = sICDate(buf, minFTermDate);
		sLogMessage("EmirReport::IsExcluded FTermGap(%d) MInFTermDate=(%s)", sLOG_INFO, 0, iFTermBKVDays, minFTermDateStr.c_str());
	}
	if ((trade->Base.Back->TermAssignStatus == sTA_TERMINATED || trade->Base.Back->TermAssignStatus == sTA_IBOR_TERMINATED) && (trade->Base.Back->TermEffDate < minFTermDate))
	{
		sLogMessage("EmirReport::IsExcluded - Operation full terminee avant le 1 jour du mois DAR", sLOG_INFO, 0);
		return true;
	}

	// Operation canceled before today
	if (trade->Base.Back->CancelDate != sMAXUS && trade->Base.Back->CancelDate < Date)
	{
		sLogMessage("EmirReport::IsExcluded - Operation cancellee avant date de traitement", sLOG_INFO, 0);
		return true;
	}

	// Exotic Trades with ExoticProdType Loan, Depo, EMTN should not be reported to EMIR
	if (trade->Base.TradeType == sTT_EXOTIC && (trade->Base.Env->cExoticType == cET_LOAN || trade->Base.Env->cExoticType == cET_DEPO || trade->Base.Env->cExoticType == cET_EMTN))
	{
		sLogMessage("EmirReport::IsExcluded - Operation Exotic decrivant un Pret/Emprunt, ExoticType = ", sLOG_INFO, 0, sICEnum("cEXO_TYPE", trade->Base.Env->cExoticType, sSHORT_ENUM));
		return true;
	}

	// Skip canceled done trades or trades that were canceled on the same date
	if (trade->Base.Env->Audit.StdAction == sACT_CANCEL)
	{
		char query[200] = "\0";
		char prevaction[7] = "\0";

		sVALLIST vlist;
		memset(&vlist, 0, sizeof(sVALLIST));

		sprintf(query, "select Audit_EntityState from dmENV_HIST where dmOwnerTable='%s' and TradeId='%s' and Audit_Version=%d", trade->Base.Env->DB.OwnerTable.Name, trade->Base.TradeID.Num, trade->Base.Env->DB.Version - 1);

		if (sDBValidList(&vlist, query, NULL, NULL, 02) == sSUCCESS && vlist.List.ItemsUsed == 1)
		{
			strcpy(prevaction, vlist.Data[0].Row);
			sEditString(NULL, prevaction, sNO_SPACES);
		}
		else
		{
			sLogMessage("EmirReport::IsExcluded - Error reading dmENV_HIST, ItemsUsed = %d ", sLOG_ERROR, 0, vlist.List.ItemsUsed);
			strcpy(prevaction, "");
		}

		sManageList(&vlist, sLIST_FREE, 0);

		if (strcmp(prevaction, "DONE") == 0 || strcmp(prevaction, "cHYP") == 0 || strcmp(prevaction, "cDHP") == 0 || strcmp(prevaction, "PTRD") == 0)
		{
			sLogMessage("EmirReport::IsExcluded - Trade done/ cancelled, not declared to DTCC", sLOG_INFO, 0);
			return true;
		}

		char buf[40];
		char aDate[10];
		sICDateTime(buf, trade->Base.Env->Audit.DateTime, 24);
		strcpy(aDate, strtok(buf, " "));

		sVALLIST vlist2;
		memset(&vlist2, 0, sizeof(sVALLIST));

		sprintf(query, "select Audit_ID from dmENV_HIST where dmOwnerTable='%s' and TradeId='%s' and Audit_Action='MKLIVE' and Audit_EntityState='VER' and TO_CHAR(Audit_DateTime,'DD/MM/YY') like '%s'", trade->Base.Env->DB.OwnerTable.Name, trade->Base.TradeID.Num, aDate);

		if (sDBValidList(&vlist2, query, NULL, NULL, 02) == sSUCCESS && vlist2.List.ItemsUsed == 1)
		{
			sLogMessage("EmirReport::IsExcluded - Trade verified and cancelled in the same date, not declared to DTCC", sLOG_INFO, 0);
			sManageList(&vlist2, sLIST_FREE, 0);
			return true;
		}
		else
		{
			sLogMessage("error reading dmENV_HIST, ItemsUsed = %d ", sLOG_ERROR, 0, vlist2.List.ItemsUsed);
		}

		sManageList(&vlist2, sLIST_FREE, 0);
	}

	return false;
}

bool AreSame(double a, double b)
{
	return fabs(a - b) < EPSILON;
}

bool EmirReport::IsDiff(const cEMRTRACT *emirtractHist, const cEMRTRACT *emirtract)
{
	// if (strcmp(emirtractHist->ReportingTimestamp.Text,  emirtract->ReportingTimestamp.Text))
	// return true;
	if (strcmp(emirtractHist->ReportSubmittingEntityID.Text, emirtract->ReportSubmittingEntityID.Text))
		return true;
	if (strcmp(emirtractHist->ReportingResponsibleEntity.Text, emirtract->ReportingResponsibleEntity.Text))
		return true;
	if (strcmp(emirtractHist->Counterparty1.Text, emirtract->Counterparty1.Text))
		return true;
	if (strcmp(emirtractHist->Counterparty1Nature.Text, emirtract->Counterparty1Nature.Text))
		return true;
	if (strcmp(emirtractHist->Counterparty1CorporateSector.Text, emirtract->Counterparty1CorporateSector.Text))
		return true;
	if (strcmp(emirtractHist->Counterparty1ClearingThreshold.Text, emirtract->Counterparty1ClearingThreshold.Text))
		return true;
	if (strcmp(emirtractHist->Counterparty2.Text, emirtract->Counterparty2.Text))
		return true;
	if (strcmp(emirtractHist->Counterparty2IDType.Text, emirtract->Counterparty2IDType.Text))
		return true;
	if (strcmp(emirtractHist->Counterparty2Country.Text, emirtract->Counterparty2Country.Text))
		return true;
	if (strcmp(emirtractHist->Counterparty2Nature.Text, emirtract->Counterparty2Nature.Text))
		return true;
	if (strcmp(emirtractHist->Counterparty2CorporateSector.Text, emirtract->Counterparty2CorporateSector.Text))
		return true;
	if (strcmp(emirtractHist->Counterparty2ClearingThreshold.Text, emirtract->Counterparty2ClearingThreshold.Text))
		return true;
	if (strcmp(emirtractHist->Ctpy2ReportingObligation.Text, emirtract->Ctpy2ReportingObligation.Text))
		return true;
	if (strcmp(emirtractHist->BrokerID.Text, emirtract->BrokerID.Text))
		return true;
	if (strcmp(emirtractHist->ClearingMember.Text, emirtract->ClearingMember.Text))
		return true;
	if (strcmp(emirtractHist->Direction.Text, emirtract->Direction.Text))
		return true;
	if (strcmp(emirtractHist->Leg1Direction.Text, emirtract->Leg1Direction.Text))
		return true;
	if (strcmp(emirtractHist->Leg2Direction.Text, emirtract->Leg2Direction.Text))
		return true;
	if (strcmp(emirtractHist->ActivityDirectlyLinked.Text, emirtract->ActivityDirectlyLinked.Text))
		return true;
	if (strcmp(emirtractHist->UTI.Text, emirtract->UTI.Text))
		return true;
	/*if (strcmp(emirtractHist->TradeID.Text,  emirtract->TradeID.Text))
		return true;*/
	if (strcmp(emirtractHist->ReportTrackingNumber.Text, emirtract->ReportTrackingNumber.Text))
		return true;
	if (strcmp(emirtractHist->PTRRID.Text, emirtract->PTRRID.Text))
		return true;
	if (strcmp(emirtractHist->PackageID.Text, emirtract->PackageID.Text))
		return true;
	if (strcmp(emirtractHist->ISIN.Text, emirtract->ISIN.Text))
		return true;
	if (strcmp(emirtractHist->UPI.Text, emirtract->UPI.Text))
		return true;
	if (strcmp(emirtractHist->ProductClass.Text, emirtract->ProductClass.Text))
		return true;
	if (strcmp(emirtractHist->ContractType.Text, emirtract->ContractType.Text))
		return true;
	if (strcmp(emirtractHist->AssetClass.Text, emirtract->AssetClass.Text))
		return true;
	if (strcmp(emirtractHist->UnderlyingIdType.Text, emirtract->UnderlyingIdType.Text))
		return true;
	if (strcmp(emirtractHist->UnderlyingId.Text, emirtract->UnderlyingId.Text))
		return true;
	// if (strcmp(emirtractHist->ValuationTimeStamp.Text,  emirtract->ValuationTimeStamp.Text))
	// return true;
	if (strcmp(emirtractHist->CollateralPortfolioIndicator.Text, emirtract->CollateralPortfolioIndicator.Text))
		return true;
	if (strcmp(emirtractHist->CollateralportfolioCode.Text, emirtract->CollateralportfolioCode.Text))
		return true;
	/*if (strcmp(emirtractHist->ConfirmationTimestamp.Text,  emirtract->ConfirmationTimestamp.Text))
		return true;*/
	if (strcmp(emirtractHist->Confirmed.Text, emirtract->Confirmed.Text))
		return true;
	if (strcmp(emirtractHist->ClearingObligation.Text, emirtract->ClearingObligation.Text))
		return true;
	if (strcmp(emirtractHist->Cleared.Text, emirtract->Cleared.Text))
		return true;
	if (strcmp(emirtractHist->ClearingTimestamp.Text, emirtract->ClearingTimestamp.Text))
		return true;
	if (strcmp(emirtractHist->CentralCounterparty.Text, emirtract->CentralCounterparty.Text))
		return true;
	if (strcmp(emirtractHist->MasterAgreementType.Text, emirtract->MasterAgreementType.Text))
		return true;
	if (strcmp(emirtractHist->OtherMasterAgreementType.Text, emirtract->OtherMasterAgreementType.Text))
		return true;
	if (strcmp(emirtractHist->MasterAgreementVersion.Text, emirtract->MasterAgreementVersion.Text))
		return true;
	if (emirtractHist->IntraGroup != emirtract->IntraGroup)
		return true;
	if (strcmp(emirtractHist->PTRR.Text, emirtract->PTRR.Text))
		return true;
	if (strcmp(emirtractHist->PTRRTechniqueType.Text, emirtract->PTRRTechniqueType.Text))
		return true;
	if (strcmp(emirtractHist->PTRRServiceProvider.Text, emirtract->PTRRServiceProvider.Text))
		return true;
	if (strcmp(emirtractHist->ExecutionVenue.Text, emirtract->ExecutionVenue.Text))
		return true;
	/*if (strcmp(emirtractHist->ExecutionTimestamp.Text,  emirtract->ExecutionTimestamp.Text))
		return true;*/
	if (strcmp(emirtractHist->EffectiveDate.Text, emirtract->EffectiveDate.Text))
		return true;
	if (strcmp(emirtractHist->ExpirationDate.Text, emirtract->ExpirationDate.Text))
		return true;
	if (strcmp(emirtractHist->EarlyTerminationDate.Text, emirtract->EarlyTerminationDate.Text))
		return true;
	if (strcmp(emirtractHist->FinalSettlementDate.Text, emirtract->FinalSettlementDate.Text))
		return true;
	if (strcmp(emirtractHist->DeliveryType.Text, emirtract->DeliveryType.Text))
		return true;
	if (strcmp(emirtractHist->Price.Text, emirtract->Price.Text))
		return true;
	if (strcmp(emirtractHist->PriceCcy.Text, emirtract->PriceCcy.Text))
		return true;
	if (strcmp(emirtractHist->Leg1NotionalAmount.Text, emirtract->Leg1NotionalAmount.Text))
		return true;
	if (strcmp(emirtractHist->NotionalCcy1.Text, emirtract->NotionalCcy1.Text))
		return true;
	if (strcmp(emirtractHist->Leg1NotionalAmtEffDate.Text, emirtract->Leg1NotionalAmtEffDate.Text))
		return true;
	if (strcmp(emirtractHist->Leg1NotionalAmountEndDate.Text, emirtract->Leg1NotionalAmountEndDate.Text))
		return true;
	if (strcmp(emirtractHist->Leg1NtlAmtAssociatedEffDate.Text, emirtract->Leg1NtlAmtAssociatedEffDate.Text))
		return true;
	if (strcmp(emirtractHist->Leg1NotionalQuantityEffDate.Text, emirtract->Leg1NotionalQuantityEffDate.Text))
		return true;
	if (strcmp(emirtractHist->Leg1NotionalQuantityEndDate.Text, emirtract->Leg1NotionalQuantityEndDate.Text))
		return true;
	if (strcmp(emirtractHist->Leg1NtlQtAssociatedEffDate.Text, emirtract->Leg1NtlQtAssociatedEffDate.Text))
		return true;
	if (strcmp(emirtractHist->Leg2NotionalAmount.Text, emirtract->Leg2NotionalAmount.Text))
		return true;
	if (strcmp(emirtractHist->NotionalCcy2.Text, emirtract->NotionalCcy2.Text))
		return true;
	if (strcmp(emirtractHist->Leg2NotionalAmtEffDate.Text, emirtract->Leg2NotionalAmtEffDate.Text))
		return true;
	if (strcmp(emirtractHist->Leg2NotionalAmountEndDate.Text, emirtract->Leg2NotionalAmountEndDate.Text))
		return true;
	if (strcmp(emirtractHist->Leg2NtlAmtAssociatedEffDate.Text, emirtract->Leg2NtlAmtAssociatedEffDate.Text))
		return true;
	if (strcmp(emirtractHist->Leg2TotalNotionalQuantity.Text, emirtract->Leg2TotalNotionalQuantity.Text))
		return true;
	if (strcmp(emirtractHist->Leg2NotionalQuantityEffDate.Text, emirtract->Leg2NotionalQuantityEffDate.Text))
		return true;
	if (strcmp(emirtractHist->Leg2NotionalQuantityEndDate.Text, emirtract->Leg2NotionalQuantityEndDate.Text))
		return true;
	if (strcmp(emirtractHist->Leg2NtlQtAssociatedEffDate.Text, emirtract->Leg2NtlQtAssociatedEffDate.Text))
		return true;
	if (strcmp(emirtractHist->Leg1FixedRate.Text, emirtract->Leg1FixedRate.Text))
		return true;
	if (strcmp(emirtractHist->Leg1FixRateDayCountConvention.Text, emirtract->Leg1FixRateDayCountConvention.Text))
		return true;
	if (strcmp(emirtractHist->Leg1FixRatePmtFrqPrd.Text, emirtract->Leg1FixRatePmtFrqPrd.Text))
		return true;
	if (strcmp(emirtractHist->Leg1FixRatePmtFrqPrdMultiplier.Text, emirtract->Leg1FixRatePmtFrqPrdMultiplier.Text))
		return true;
	if (strcmp(emirtractHist->Leg1FloatingRateIdentifier.Text, emirtract->Leg1FloatingRateIdentifier.Text))
		return true;
	if (strcmp(emirtractHist->Leg1FloatingRateIndicator.Text, emirtract->Leg1FloatingRateIndicator.Text))
		return true;
	if (strcmp(emirtractHist->Leg1FloatingRateName.Text, emirtract->Leg1FloatingRateName.Text))
		return true;
	if (strcmp(emirtractHist->Leg1FltRateDayCountConvention.Text, emirtract->Leg1FltRateDayCountConvention.Text))
		return true;
	if (strcmp(emirtractHist->Leg1FltRatePmtFrqPrd.Text, emirtract->Leg1FltRatePmtFrqPrd.Text))
		return true;
	if (strcmp(emirtractHist->Leg1FltRatePmtFrqPrdMultiplier.Text, emirtract->Leg1FltRatePmtFrqPrdMultiplier.Text))
		return true;
	if (strcmp(emirtractHist->Leg1FltRateRefPrdTimePrd.Text, emirtract->Leg1FltRateRefPrdTimePrd.Text))
		return true;
	if (strcmp(emirtractHist->Leg1FltRateRefPrdMultiplier.Text, emirtract->Leg1FltRateRefPrdMultiplier.Text))
		return true;
	if (strcmp(emirtractHist->Leg1FltRateResetFrqPrd.Text, emirtract->Leg1FltRateResetFrqPrd.Text))
		return true;
	if (strcmp(emirtractHist->Leg1FltRateResetFrqMultiplier.Text, emirtract->Leg1FltRateResetFrqMultiplier.Text))
		return true;
	if (strcmp(emirtractHist->Leg2FixedRate.Text, emirtract->Leg2FixedRate.Text))
		return true;
	if (strcmp(emirtractHist->Leg2FixRateDayCountConvention.Text, emirtract->Leg2FixRateDayCountConvention.Text))
		return true;
	if (strcmp(emirtractHist->Leg2FixRatePmtFrqPrd.Text, emirtract->Leg2FixRatePmtFrqPrd.Text))
		return true;
	if (strcmp(emirtractHist->Leg2FixRatePmtFrqPrdMultiplier.Text, emirtract->Leg2FixRatePmtFrqPrdMultiplier.Text))
		return true;
	if (strcmp(emirtractHist->Leg2FloatingRateIdentifier.Text, emirtract->Leg2FloatingRateIdentifier.Text))
		return true;
	if (strcmp(emirtractHist->Leg2FloatingRateIndicator.Text, emirtract->Leg2FloatingRateIndicator.Text))
		return true;
	if (strcmp(emirtractHist->Leg2FloatingRateName.Text, emirtract->Leg2FloatingRateName.Text))
		return true;
	if (strcmp(emirtractHist->Leg2FltRateDayCountConvention.Text, emirtract->Leg2FltRateDayCountConvention.Text))
		return true;
	if (strcmp(emirtractHist->Leg2FltRatePmtFrqPrd.Text, emirtract->Leg2FltRatePmtFrqPrd.Text))
		return true;
	if (strcmp(emirtractHist->Leg2FltRatePmtFrqPrdMultiplier.Text, emirtract->Leg2FltRatePmtFrqPrdMultiplier.Text))
		return true;
	if (strcmp(emirtractHist->Leg2FltRateRefPrdTimePrd.Text, emirtract->Leg2FltRateRefPrdTimePrd.Text))
		return true;
	if (strcmp(emirtractHist->Leg2FltRateRefPrdMultiplier.Text, emirtract->Leg2FltRateRefPrdMultiplier.Text))
		return true;
	if (strcmp(emirtractHist->Leg2FltRateResetFrqPrd.Text, emirtract->Leg2FltRateResetFrqPrd.Text))
		return true;
	if (strcmp(emirtractHist->Leg2FltRateResetFrqMultiplier.Text, emirtract->Leg2FltRateResetFrqMultiplier.Text))
		return true;
	if (strcmp(emirtractHist->OptionType.Text, emirtract->OptionType.Text))
		return true;
	if (strcmp(emirtractHist->OptionStyle.Text, emirtract->OptionStyle.Text))
		return true;
	if (strcmp(emirtractHist->UnderlyingMaturityDate.Text, emirtract->UnderlyingMaturityDate.Text))
		return true;
	if (strcmp(emirtractHist->Seniority.Text, emirtract->Seniority.Text))
		return true;
	if (strcmp(emirtractHist->ReferenceEntity.Text, emirtract->ReferenceEntity.Text))
		return true;
	if (strcmp(emirtractHist->IndexFactor.Text, emirtract->IndexFactor.Text))
		return true;
	if (strcmp(emirtractHist->Tranche.Text, emirtract->Tranche.Text))
		return true;
	// if (strcmp(emirtractHist->ProcessDate.Text,  emirtract->ProcessDate.Text))
	// return true;
	// if (strcmp(emirtractHist->PriorUTI.Text,  emirtract->PriorUTI.Text))
	// return true;
	// if (strcmp(emirtractHist->SubPositionUTI.Text,  emirtract->SubPositionUTI.Text))
	// return true;
	// if (strcmp(emirtractHist->DerivativeCryptoAssets.Text,  emirtract->DerivativeCryptoAssets.Text))
	// return true;
	// if (strcmp(emirtractHist->UnderlyingIdx.Text,  emirtract->UnderlyingIdx.Text))
	// return true;
	// if (strcmp(emirtractHist->UnderlyingIdxName.Text,  emirtract->UnderlyingIdxName.Text))
	// return true;
	// if (strcmp(emirtractHist->CustomBasketCd.Text,  emirtract->CustomBasketCd.Text))
	// return true;
	// if (strcmp(emirtractHist->BasketConstituentsID.Text,  emirtract->BasketConstituentsID.Text))
	// return true;
	// if (strcmp(emirtractHist->SettlementCcy1.Text,  emirtract->SettlementCcy1.Text))
	// return true;
	// if (strcmp(emirtractHist->SettlementCcy2.Text,  emirtract->SettlementCcy2.Text))
	// return true;
	// if (strcmp(emirtractHist->ValuationAmount.Text,  emirtract->ValuationAmount.Text))
	// return true;
	// if (strcmp(emirtractHist->ValuationCcy.Text,  emirtract->ValuationCcy.Text))
	// return true;
	// if (strcmp(emirtractHist->ValuationMthd.Text,  emirtract->ValuationMthd.Text))
	// return true;
	// if (emirtractHist->Delta == emirtract->Delta)
	// return true;
	// if (strcmp(emirtractHist->PriceUnadjustedEffDate.Text,  emirtract->PriceUnadjustedEffDate.Text))
	// return true;
	// if (strcmp(emirtractHist->PriceUnadjustedEndDate.Text,  emirtract->PriceUnadjustedEndDate.Text))
	// return true;
	// if (strcmp(emirtractHist->PriceInUnAdjEffAndEndDate.Text,  emirtract->PriceInUnAdjEffAndEndDate.Text))
	// return true;
	// if (strcmp(emirtractHist->PackageTransactionPrice.Text,  emirtract->PackageTransactionPrice.Text))
	// return true;
	// if (strcmp(emirtractHist->PackageTransactionPriceCcy.Text,  emirtract->PackageTransactionPriceCcy.Text))
	// return true;
	// if (strcmp(emirtractHist->OtherPaymentType.Text,  emirtract->OtherPaymentType.Text))
	// return true;
	// if (strcmp(emirtractHist->OtherPaymentAmount.Text,  emirtract->OtherPaymentAmount.Text))
	// return true;
	// if (strcmp(emirtractHist->OtherPaymentCcy.Text,  emirtract->OtherPaymentCcy.Text))
	// return true;
	// if (strcmp(emirtractHist->OtherPaymentDate.Text,  emirtract->OtherPaymentDate.Text))
	// return true;
	// if (strcmp(emirtractHist->OtherPaymentPayer.Text,  emirtract->OtherPaymentPayer.Text))
	// return true;
	// if (strcmp(emirtractHist->OtherPaymentreceiver.Text,  emirtract->OtherPaymentreceiver.Text))
	// return true;
	// if (strcmp(emirtractHist->Leg1Spread.Text,  emirtract->Leg1Spread.Text))
	// return true;
	// if (strcmp(emirtractHist->Leg1SpreadCcy.Text,  emirtract->Leg1SpreadCcy.Text))
	// return true;
	// if (strcmp(emirtractHist->StrikePrice.Text,  emirtract->StrikePrice.Text))
	// return true;
	// if (strcmp(emirtractHist->StrikePriceEffectiveDate.Text,  emirtract->StrikePriceEffectiveDate.Text))
	// return true;
	// if (strcmp(emirtractHist->StrikePriceEndDate.Text,  emirtract->StrikePriceEndDate.Text))
	// return true;
	// if (strcmp(emirtractHist->StrikePriceAssociatedEffDate.Text,  emirtract->StrikePriceAssociatedEffDate.Text))
	// return true;
	// if (strcmp(emirtractHist->StrikePriceCcy.Text,  emirtract->StrikePriceCcy.Text))
	// return true;
	// if (strcmp(emirtractHist->OptionPremiumAmount.Text,  emirtract->OptionPremiumAmount.Text))
	// return true;
	// if (strcmp(emirtractHist->OptionPremiumCcy.Text,  emirtract->OptionPremiumCcy.Text))
	// return true;
	// if (strcmp(emirtractHist->OptionPremiumPaymentDate.Text,  emirtract->OptionPremiumPaymentDate.Text))
	// return true;
	// if (strcmp(emirtractHist->CDSIdxAttachmenPoint.Text,  emirtract->CDSIdxAttachmenPoint.Text))
	// return true;
	// if (strcmp(emirtractHist->CDSIdxDetachmenPoint.Text,  emirtract->CDSIdxDetachmenPoint.Text))
	// return true;
	// if (strcmp(emirtractHist->SubProduct.Text,  emirtract->SubProduct.Text))
	// return true;
	// if (strcmp(emirtractHist->Series.Text,  emirtract->Series.Text))
	// return true;
	// if (emirtractHist->cVersion == emirtract->cVersion)
	// return true;
	// if (strcmp(emirtractHist->Leg2Spread.Text,  emirtract->Leg2Spread.Text))
	// return true;
	// if (strcmp(emirtractHist->Leg2SpreadCcy.Text,  emirtract->Leg2SpreadCcy.Text))
	// return true;
	// if (strcmp(emirtractHist->PackageTransactionSpread.Text,  emirtract->PackageTransactionSpread.Text))
	// return true;
	// if (strcmp(emirtractHist->PackageTransactionSpreadCcy.Text,  emirtract->PackageTransactionSpreadCcy.Text))
	// return true;
	// if (strcmp(emirtractHist->ExchangeRate1.Text,  emirtract->ExchangeRate1.Text))
	// return true;
	// if (strcmp(emirtractHist->ForwardExchangeRate.Text,  emirtract->ForwardExchangeRate.Text))
	// return true;
	// if (strcmp(emirtractHist->ExchangeRateBasis.Text,  emirtract->ExchangeRateBasis.Text))
	// return true;
	// if (strcmp(emirtractHist->BaseProduct.Text,  emirtract->BaseProduct.Text))
	// return true;
	// if (strcmp(emirtractHist->FurtherSubProduct.Text,  emirtract->FurtherSubProduct.Text))
	// return true;
	// if (strcmp(emirtractHist->DeliveryPoint.Text,  emirtract->DeliveryPoint.Text))
	// return true;
	// if (strcmp(emirtractHist->InterConnectionPoint.Text,  emirtract->InterConnectionPoint.Text))
	// return true;
	// if (strcmp(emirtractHist->LoadType.Text,  emirtract->LoadType.Text))
	// return true;
	// if (strcmp(emirtractHist->DeliveryIntervalStartTime.Text,  emirtract->DeliveryIntervalStartTime.Text))
	// return true;
	// if (strcmp(emirtractHist->DeliveryIntervalEndTime.Text,  emirtract->DeliveryIntervalEndTime.Text))
	// return true;
	// if (strcmp(emirtractHist->DeliveryStartDate.Text,  emirtract->DeliveryStartDate.Text))
	// return true;
	// if (strcmp(emirtractHist->DeliveryEndDate.Text,  emirtract->DeliveryEndDate.Text))
	// return true;
	// if (emirtractHist->cDuration == emirtract->cDuration)
	// return true;
	// if (emirtractHist->WeekDays == emirtract->WeekDays)
	// return true;
	// if (strcmp(emirtractHist->DeliveryCapacity.Text,  emirtract->DeliveryCapacity.Text))
	// return true;
	// if (strcmp(emirtractHist->QuantityUnit.Text,  emirtract->QuantityUnit.Text))
	// return true;
	// if (strcmp(emirtractHist->PriceTimeintervalQuantity.Text,  emirtract->PriceTimeintervalQuantity.Text))
	// return true;
	// if (strcmp(emirtractHist->PriceCcyTimeIntervalQuantity.Text,  emirtract->PriceCcyTimeIntervalQuantity.Text))
	// return true;

	return false;
}

bool EmirReport::CopyDiff(cEMRTRACT *emirtractHist, const cEMRTRACT *emirtract)
{
	if (strcmp(emirtractHist->ReportingTimestamp.Text, emirtract->ReportingTimestamp.Text))
	{
		strcpy(emirtractHist->ReportingTimestamp.Text, emirtract->ReportingTimestamp.Text);
	}
	if (strcmp(emirtractHist->ReportSubmittingEntityID.Text, emirtract->ReportSubmittingEntityID.Text))
	{
		strcpy(emirtractHist->ReportSubmittingEntityID.Text, emirtract->ReportSubmittingEntityID.Text);
	}
	if (strcmp(emirtractHist->ReportingResponsibleEntity.Text, emirtract->ReportingResponsibleEntity.Text))
	{
		strcpy(emirtractHist->ReportingResponsibleEntity.Text, emirtract->ReportingResponsibleEntity.Text);
	}
	if (strcmp(emirtractHist->Counterparty1.Text, emirtract->Counterparty1.Text))
	{
		strcpy(emirtractHist->Counterparty1.Text, emirtract->Counterparty1.Text);
	}
	if (strcmp(emirtractHist->Counterparty1Nature.Text, emirtract->Counterparty1Nature.Text))
	{
		strcpy(emirtractHist->Counterparty1Nature.Text, emirtract->Counterparty1Nature.Text);
	}
	if (strcmp(emirtractHist->Counterparty1CorporateSector.Text, emirtract->Counterparty1CorporateSector.Text))
	{
		strcpy(emirtractHist->Counterparty1CorporateSector.Text, emirtract->Counterparty1CorporateSector.Text);
	}
	if (strcmp(emirtractHist->Counterparty1ClearingThreshold.Text, emirtract->Counterparty1ClearingThreshold.Text))
	{
		strcpy(emirtractHist->Counterparty1ClearingThreshold.Text, emirtract->Counterparty1ClearingThreshold.Text);
	}
	if (strcmp(emirtractHist->Counterparty2.Text, emirtract->Counterparty2.Text))
	{
		strcpy(emirtractHist->Counterparty2.Text, emirtract->Counterparty2.Text);
	}
	if (strcmp(emirtractHist->Counterparty2IDType.Text, emirtract->Counterparty2IDType.Text))
	{
		strcpy(emirtractHist->Counterparty2IDType.Text, emirtract->Counterparty2IDType.Text);
	}
	if (strcmp(emirtractHist->Counterparty2Country.Text, emirtract->Counterparty2Country.Text))
	{
		strcpy(emirtractHist->Counterparty2Country.Text, emirtract->Counterparty2Country.Text);
	}
	if (strcmp(emirtractHist->Counterparty2Nature.Text, emirtract->Counterparty2Nature.Text))
	{
		strcpy(emirtractHist->Counterparty2Nature.Text, emirtract->Counterparty2Nature.Text);
	}
	if (strcmp(emirtractHist->Counterparty2CorporateSector.Text, emirtract->Counterparty2CorporateSector.Text))
	{
		strcpy(emirtractHist->Counterparty2CorporateSector.Text, emirtract->Counterparty2CorporateSector.Text);
	}
	if (strcmp(emirtractHist->Counterparty2ClearingThreshold.Text, emirtract->Counterparty2ClearingThreshold.Text))
	{
		strcpy(emirtractHist->Counterparty2ClearingThreshold.Text, emirtract->Counterparty2ClearingThreshold.Text);
	}
	if (strcmp(emirtractHist->Ctpy2ReportingObligation.Text, emirtract->Ctpy2ReportingObligation.Text))
	{
		strcpy(emirtractHist->Ctpy2ReportingObligation.Text, emirtract->Ctpy2ReportingObligation.Text);
	}
	if (strcmp(emirtractHist->BrokerID.Text, emirtract->BrokerID.Text))
	{
		strcpy(emirtractHist->BrokerID.Text, emirtract->BrokerID.Text);
	}
	if (strcmp(emirtractHist->ClearingMember.Text, emirtract->ClearingMember.Text))
	{
		strcpy(emirtractHist->ClearingMember.Text, emirtract->ClearingMember.Text);
	}
	if (strcmp(emirtractHist->Direction.Text, emirtract->Direction.Text))
	{
		strcpy(emirtractHist->Direction.Text, emirtract->Direction.Text);
	}
	if (strcmp(emirtractHist->Leg1Direction.Text, emirtract->Leg1Direction.Text))
	{
		strcpy(emirtractHist->Leg1Direction.Text, emirtract->Leg1Direction.Text);
	}
	if (strcmp(emirtractHist->Leg2Direction.Text, emirtract->Leg2Direction.Text))
	{
		strcpy(emirtractHist->Leg2Direction.Text, emirtract->Leg2Direction.Text);
	}
	if (strcmp(emirtractHist->ActivityDirectlyLinked.Text, emirtract->ActivityDirectlyLinked.Text))
	{
		strcpy(emirtractHist->ActivityDirectlyLinked.Text, emirtract->ActivityDirectlyLinked.Text);
	}
	if (strcmp(emirtractHist->UTI.Text, emirtract->UTI.Text))
	{
		strcpy(emirtractHist->UTI.Text, emirtract->UTI.Text);
	}
	/*if(strcmp(emirtractHist->TradeID.Text,  emirtract->TradeID.Text)) {
		strcpy(emirtractHist->TradeID.Text,  emirtract->TradeID.Text);
	}*/
	if (strcmp(emirtractHist->ReportTrackingNumber.Text, emirtract->ReportTrackingNumber.Text))
	{
		strcpy(emirtractHist->ReportTrackingNumber.Text, emirtract->ReportTrackingNumber.Text);
	}
	if (strcmp(emirtractHist->PTRRID.Text, emirtract->PTRRID.Text))
	{
		strcpy(emirtractHist->PTRRID.Text, emirtract->PTRRID.Text);
	}
	if (strcmp(emirtractHist->PackageID.Text, emirtract->PackageID.Text))
	{
		strcpy(emirtractHist->PackageID.Text, emirtract->PackageID.Text);
	}
	if (strcmp(emirtractHist->ISIN.Text, emirtract->ISIN.Text))
	{
		strcpy(emirtractHist->ISIN.Text, emirtract->ISIN.Text);
	}
	if (strcmp(emirtractHist->UPI.Text, emirtract->UPI.Text))
	{
		strcpy(emirtractHist->UPI.Text, emirtract->UPI.Text);
	}
	if (strcmp(emirtractHist->ProductClass.Text, emirtract->ProductClass.Text))
	{
		strcpy(emirtractHist->ProductClass.Text, emirtract->ProductClass.Text);
	}
	if (strcmp(emirtractHist->ContractType.Text, emirtract->ContractType.Text))
	{
		strcpy(emirtractHist->ContractType.Text, emirtract->ContractType.Text);
	}
	if (strcmp(emirtractHist->AssetClass.Text, emirtract->AssetClass.Text))
	{
		strcpy(emirtractHist->AssetClass.Text, emirtract->AssetClass.Text);
	}
	if (strcmp(emirtractHist->UnderlyingIdType.Text, emirtract->UnderlyingIdType.Text))
	{
		strcpy(emirtractHist->UnderlyingIdType.Text, emirtract->UnderlyingIdType.Text);
	}
	if (strcmp(emirtractHist->UnderlyingId.Text, emirtract->UnderlyingId.Text))
	{
		strcpy(emirtractHist->UnderlyingId.Text, emirtract->UnderlyingId.Text);
	}
	if (strcmp(emirtractHist->ValuationTimeStamp.Text, emirtract->ValuationTimeStamp.Text))
	{
		strcpy(emirtractHist->ValuationTimeStamp.Text, emirtract->ValuationTimeStamp.Text);
	}
	if (strcmp(emirtractHist->CollateralPortfolioIndicator.Text, emirtract->CollateralPortfolioIndicator.Text))
	{
		strcpy(emirtractHist->CollateralPortfolioIndicator.Text, emirtract->CollateralPortfolioIndicator.Text);
	}
	if (strcmp(emirtractHist->CollateralportfolioCode.Text, emirtract->CollateralportfolioCode.Text))
	{
		strcpy(emirtractHist->CollateralportfolioCode.Text, emirtract->CollateralportfolioCode.Text);
	}
	/*if(strcmp(emirtractHist->ConfirmationTimestamp.Text,  emirtract->ConfirmationTimestamp.Text)) {
		strcpy(emirtractHist->ConfirmationTimestamp.Text,  emirtract->ConfirmationTimestamp.Text);
	}*/
	if (strcmp(emirtractHist->Confirmed.Text, emirtract->Confirmed.Text))
	{
		strcpy(emirtractHist->Confirmed.Text, emirtract->Confirmed.Text);
	}
	if (strcmp(emirtractHist->ClearingObligation.Text, emirtract->ClearingObligation.Text))
	{
		strcpy(emirtractHist->ClearingObligation.Text, emirtract->ClearingObligation.Text);
	}
	if (strcmp(emirtractHist->Cleared.Text, emirtract->Cleared.Text))
	{
		strcpy(emirtractHist->Cleared.Text, emirtract->Cleared.Text);
	}
	if (strcmp(emirtractHist->ClearingTimestamp.Text, emirtract->ClearingTimestamp.Text))
	{
		strcpy(emirtractHist->ClearingTimestamp.Text, emirtract->ClearingTimestamp.Text);
	}
	if (strcmp(emirtractHist->CentralCounterparty.Text, emirtract->CentralCounterparty.Text))
	{
		strcpy(emirtractHist->CentralCounterparty.Text, emirtract->CentralCounterparty.Text);
	}
	if (strcmp(emirtractHist->MasterAgreementType.Text, emirtract->MasterAgreementType.Text))
	{
		strcpy(emirtractHist->MasterAgreementType.Text, emirtract->MasterAgreementType.Text);
	}
	if (strcmp(emirtractHist->OtherMasterAgreementType.Text, emirtract->OtherMasterAgreementType.Text))
	{
		strcpy(emirtractHist->OtherMasterAgreementType.Text, emirtract->OtherMasterAgreementType.Text);
	}
	if (strcmp(emirtractHist->MasterAgreementVersion.Text, emirtract->MasterAgreementVersion.Text))
	{
		strcpy(emirtractHist->MasterAgreementVersion.Text, emirtract->MasterAgreementVersion.Text);
	}
	if (emirtractHist->IntraGroup != emirtract->IntraGroup)
	{
		emirtractHist->IntraGroup = emirtract->IntraGroup;
	}
	if (strcmp(emirtractHist->PTRR.Text, emirtract->PTRR.Text))
	{
		strcpy(emirtractHist->PTRR.Text, emirtract->PTRR.Text);
	}
	if (strcmp(emirtractHist->PTRRTechniqueType.Text, emirtract->PTRRTechniqueType.Text))
	{
		strcpy(emirtractHist->PTRRTechniqueType.Text, emirtract->PTRRTechniqueType.Text);
	}
	if (strcmp(emirtractHist->PTRRServiceProvider.Text, emirtract->PTRRServiceProvider.Text))
	{
		strcpy(emirtractHist->PTRRServiceProvider.Text, emirtract->PTRRServiceProvider.Text);
	}
	if (strcmp(emirtractHist->ExecutionVenue.Text, emirtract->ExecutionVenue.Text))
	{
		strcpy(emirtractHist->ExecutionVenue.Text, emirtract->ExecutionVenue.Text);
	}
	/*if(strcmp(emirtractHist->ExecutionTimestamp.Text,  emirtract->ExecutionTimestamp.Text)) {
		strcpy(emirtractHist->ExecutionTimestamp.Text,  emirtract->ExecutionTimestamp.Text);
	}*/
	if (strcmp(emirtractHist->EffectiveDate.Text, emirtract->EffectiveDate.Text))
	{
		strcpy(emirtractHist->EffectiveDate.Text, emirtract->EffectiveDate.Text);
	}
	if (strcmp(emirtractHist->ExpirationDate.Text, emirtract->ExpirationDate.Text))
	{
		strcpy(emirtractHist->ExpirationDate.Text, emirtract->ExpirationDate.Text);
	}
	if (strcmp(emirtractHist->EarlyTerminationDate.Text, emirtract->EarlyTerminationDate.Text))
	{
		strcpy(emirtractHist->EarlyTerminationDate.Text, emirtract->EarlyTerminationDate.Text);
	}
	if (strcmp(emirtractHist->FinalSettlementDate.Text, emirtract->FinalSettlementDate.Text))
	{
		strcpy(emirtractHist->FinalSettlementDate.Text, emirtract->FinalSettlementDate.Text);
	}
	if (strcmp(emirtractHist->DeliveryType.Text, emirtract->DeliveryType.Text))
	{
		strcpy(emirtractHist->DeliveryType.Text, emirtract->DeliveryType.Text);
	}
	if (strcmp(emirtractHist->Price.Text, emirtract->Price.Text))
	{
		strcpy(emirtractHist->Price.Text, emirtract->Price.Text);
	}
	if (strcmp(emirtractHist->PriceCcy.Text, emirtract->PriceCcy.Text))
	{
		strcpy(emirtractHist->PriceCcy.Text, emirtract->PriceCcy.Text);
	}
	if (strcmp(emirtractHist->Leg1NotionalAmount.Text, emirtract->Leg1NotionalAmount.Text))
	{
		strcpy(emirtractHist->Leg1NotionalAmount.Text, emirtract->Leg1NotionalAmount.Text);
	}
	if (strcmp(emirtractHist->NotionalCcy1.Text, emirtract->NotionalCcy1.Text))
	{
		strcpy(emirtractHist->NotionalCcy1.Text, emirtract->NotionalCcy1.Text);
	}
	if (strcmp(emirtractHist->Leg1NotionalAmtEffDate.Text, emirtract->Leg1NotionalAmtEffDate.Text))
	{
		strcpy(emirtractHist->Leg1NotionalAmtEffDate.Text, emirtract->Leg1NotionalAmtEffDate.Text);
	}
	if (strcmp(emirtractHist->Leg1NotionalAmountEndDate.Text, emirtract->Leg1NotionalAmountEndDate.Text))
	{
		strcpy(emirtractHist->Leg1NotionalAmountEndDate.Text, emirtract->Leg1NotionalAmountEndDate.Text);
	}
	if (strcmp(emirtractHist->Leg1NtlAmtAssociatedEffDate.Text, emirtract->Leg1NtlAmtAssociatedEffDate.Text))
	{
		strcpy(emirtractHist->Leg1NtlAmtAssociatedEffDate.Text, emirtract->Leg1NtlAmtAssociatedEffDate.Text);
	}
	if (strcmp(emirtractHist->Leg1NotionalQuantityEffDate.Text, emirtract->Leg1NotionalQuantityEffDate.Text))
	{
		strcpy(emirtractHist->Leg1NotionalQuantityEffDate.Text, emirtract->Leg1NotionalQuantityEffDate.Text);
	}
	if (strcmp(emirtractHist->Leg1NotionalQuantityEndDate.Text, emirtract->Leg1NotionalQuantityEndDate.Text))
	{
		strcpy(emirtractHist->Leg1NotionalQuantityEndDate.Text, emirtract->Leg1NotionalQuantityEndDate.Text);
	}
	if (strcmp(emirtractHist->Leg1NtlQtAssociatedEffDate.Text, emirtract->Leg1NtlQtAssociatedEffDate.Text))
	{
		strcpy(emirtractHist->Leg1NtlQtAssociatedEffDate.Text, emirtract->Leg1NtlQtAssociatedEffDate.Text);
	}
	if (strcmp(emirtractHist->Leg2NotionalAmount.Text, emirtract->Leg2NotionalAmount.Text))
	{
		strcpy(emirtractHist->Leg2NotionalAmount.Text, emirtract->Leg2NotionalAmount.Text);
	}
	if (strcmp(emirtractHist->NotionalCcy2.Text, emirtract->NotionalCcy2.Text))
	{
		strcpy(emirtractHist->NotionalCcy2.Text, emirtract->NotionalCcy2.Text);
	}
	if (strcmp(emirtractHist->Leg2NotionalAmtEffDate.Text, emirtract->Leg2NotionalAmtEffDate.Text))
	{
		strcpy(emirtractHist->Leg2NotionalAmtEffDate.Text, emirtract->Leg2NotionalAmtEffDate.Text);
	}
	if (strcmp(emirtractHist->Leg2NotionalAmountEndDate.Text, emirtract->Leg2NotionalAmountEndDate.Text))
	{
		strcpy(emirtractHist->Leg2NotionalAmountEndDate.Text, emirtract->Leg2NotionalAmountEndDate.Text);
	}
	if (strcmp(emirtractHist->Leg2NtlAmtAssociatedEffDate.Text, emirtract->Leg2NtlAmtAssociatedEffDate.Text))
	{
		strcpy(emirtractHist->Leg2NtlAmtAssociatedEffDate.Text, emirtract->Leg2NtlAmtAssociatedEffDate.Text);
	}
	if (strcmp(emirtractHist->Leg2TotalNotionalQuantity.Text, emirtract->Leg2TotalNotionalQuantity.Text))
	{
		strcpy(emirtractHist->Leg2TotalNotionalQuantity.Text, emirtract->Leg2TotalNotionalQuantity.Text);
	}
	if (strcmp(emirtractHist->Leg2NotionalQuantityEffDate.Text, emirtract->Leg2NotionalQuantityEffDate.Text))
	{
		strcpy(emirtractHist->Leg2NotionalQuantityEffDate.Text, emirtract->Leg2NotionalQuantityEffDate.Text);
	}
	if (strcmp(emirtractHist->Leg2NotionalQuantityEndDate.Text, emirtract->Leg2NotionalQuantityEndDate.Text))
	{
		strcpy(emirtractHist->Leg2NotionalQuantityEndDate.Text, emirtract->Leg2NotionalQuantityEndDate.Text);
	}
	if (strcmp(emirtractHist->Leg2NtlQtAssociatedEffDate.Text, emirtract->Leg2NtlQtAssociatedEffDate.Text))
	{
		strcpy(emirtractHist->Leg2NtlQtAssociatedEffDate.Text, emirtract->Leg2NtlQtAssociatedEffDate.Text);
	}
	if (strcmp(emirtractHist->Leg1FixedRate.Text, emirtract->Leg1FixedRate.Text))
	{
		strcpy(emirtractHist->Leg1FixedRate.Text, emirtract->Leg1FixedRate.Text);
	}
	if (strcmp(emirtractHist->Leg1FixRateDayCountConvention.Text, emirtract->Leg1FixRateDayCountConvention.Text))
	{
		strcpy(emirtractHist->Leg1FixRateDayCountConvention.Text, emirtract->Leg1FixRateDayCountConvention.Text);
	}
	if (strcmp(emirtractHist->Leg1FixRatePmtFrqPrd.Text, emirtract->Leg1FixRatePmtFrqPrd.Text))
	{
		strcpy(emirtractHist->Leg1FixRatePmtFrqPrd.Text, emirtract->Leg1FixRatePmtFrqPrd.Text);
	}
	if (strcmp(emirtractHist->Leg1FixRatePmtFrqPrdMultiplier.Text, emirtract->Leg1FixRatePmtFrqPrdMultiplier.Text))
	{
		strcpy(emirtractHist->Leg1FixRatePmtFrqPrdMultiplier.Text, emirtract->Leg1FixRatePmtFrqPrdMultiplier.Text);
	}
	if (strcmp(emirtractHist->Leg1FloatingRateIdentifier.Text, emirtract->Leg1FloatingRateIdentifier.Text))
	{
		strcpy(emirtractHist->Leg1FloatingRateIdentifier.Text, emirtract->Leg1FloatingRateIdentifier.Text);
	}
	if (strcmp(emirtractHist->Leg1FloatingRateIndicator.Text, emirtract->Leg1FloatingRateIndicator.Text))
	{
		strcpy(emirtractHist->Leg1FloatingRateIndicator.Text, emirtract->Leg1FloatingRateIndicator.Text);
	}
	if (strcmp(emirtractHist->Leg1FloatingRateName.Text, emirtract->Leg1FloatingRateName.Text))
	{
		strcpy(emirtractHist->Leg1FloatingRateName.Text, emirtract->Leg1FloatingRateName.Text);
	}
	if (strcmp(emirtractHist->Leg1FltRateDayCountConvention.Text, emirtract->Leg1FltRateDayCountConvention.Text))
	{
		strcpy(emirtractHist->Leg1FltRateDayCountConvention.Text, emirtract->Leg1FltRateDayCountConvention.Text);
	}
	if (strcmp(emirtractHist->Leg1FltRatePmtFrqPrd.Text, emirtract->Leg1FltRatePmtFrqPrd.Text))
	{
		strcpy(emirtractHist->Leg1FltRatePmtFrqPrd.Text, emirtract->Leg1FltRatePmtFrqPrd.Text);
	}
	if (strcmp(emirtractHist->Leg1FltRatePmtFrqPrdMultiplier.Text, emirtract->Leg1FltRatePmtFrqPrdMultiplier.Text))
	{
		strcpy(emirtractHist->Leg1FltRatePmtFrqPrdMultiplier.Text, emirtract->Leg1FltRatePmtFrqPrdMultiplier.Text);
	}
	if (strcmp(emirtractHist->Leg1FltRateRefPrdTimePrd.Text, emirtract->Leg1FltRateRefPrdTimePrd.Text))
	{
		strcpy(emirtractHist->Leg1FltRateRefPrdTimePrd.Text, emirtract->Leg1FltRateRefPrdTimePrd.Text);
	}
	if (strcmp(emirtractHist->Leg1FltRateRefPrdMultiplier.Text, emirtract->Leg1FltRateRefPrdMultiplier.Text))
	{
		strcpy(emirtractHist->Leg1FltRateRefPrdMultiplier.Text, emirtract->Leg1FltRateRefPrdMultiplier.Text);
	}
	if (strcmp(emirtractHist->Leg1FltRateResetFrqPrd.Text, emirtract->Leg1FltRateResetFrqPrd.Text))
	{
		strcpy(emirtractHist->Leg1FltRateResetFrqPrd.Text, emirtract->Leg1FltRateResetFrqPrd.Text);
	}
	if (strcmp(emirtractHist->Leg1FltRateResetFrqMultiplier.Text, emirtract->Leg1FltRateResetFrqMultiplier.Text))
	{
		strcpy(emirtractHist->Leg1FltRateResetFrqMultiplier.Text, emirtract->Leg1FltRateResetFrqMultiplier.Text);
	}
	if (strcmp(emirtractHist->Leg2FixedRate.Text, emirtract->Leg2FixedRate.Text))
	{
		strcpy(emirtractHist->Leg2FixedRate.Text, emirtract->Leg2FixedRate.Text);
	}
	if (strcmp(emirtractHist->Leg2FixRateDayCountConvention.Text, emirtract->Leg2FixRateDayCountConvention.Text))
	{
		strcpy(emirtractHist->Leg2FixRateDayCountConvention.Text, emirtract->Leg2FixRateDayCountConvention.Text);
	}
	if (strcmp(emirtractHist->Leg2FixRatePmtFrqPrd.Text, emirtract->Leg2FixRatePmtFrqPrd.Text))
	{
		strcpy(emirtractHist->Leg2FixRatePmtFrqPrd.Text, emirtract->Leg2FixRatePmtFrqPrd.Text);
	}
	if (strcmp(emirtractHist->Leg2FixRatePmtFrqPrdMultiplier.Text, emirtract->Leg2FixRatePmtFrqPrdMultiplier.Text))
	{
		strcpy(emirtractHist->Leg2FixRatePmtFrqPrdMultiplier.Text, emirtract->Leg2FixRatePmtFrqPrdMultiplier.Text);
	}
	if (strcmp(emirtractHist->Leg2FloatingRateIdentifier.Text, emirtract->Leg2FloatingRateIdentifier.Text))
	{
		strcpy(emirtractHist->Leg2FloatingRateIdentifier.Text, emirtract->Leg2FloatingRateIdentifier.Text);
	}
	if (strcmp(emirtractHist->Leg2FloatingRateIndicator.Text, emirtract->Leg2FloatingRateIndicator.Text))
	{
		strcpy(emirtractHist->Leg2FloatingRateIndicator.Text, emirtract->Leg2FloatingRateIndicator.Text);
	}
	if (strcmp(emirtractHist->Leg2FloatingRateName.Text, emirtract->Leg2FloatingRateName.Text))
	{
		strcpy(emirtractHist->Leg2FloatingRateName.Text, emirtract->Leg2FloatingRateName.Text);
	}
	if (strcmp(emirtractHist->Leg2FltRateDayCountConvention.Text, emirtract->Leg2FltRateDayCountConvention.Text))
	{
		strcpy(emirtractHist->Leg2FltRateDayCountConvention.Text, emirtract->Leg2FltRateDayCountConvention.Text);
	}
	if (strcmp(emirtractHist->Leg2FltRatePmtFrqPrd.Text, emirtract->Leg2FltRatePmtFrqPrd.Text))
	{
		strcpy(emirtractHist->Leg2FltRatePmtFrqPrd.Text, emirtract->Leg2FltRatePmtFrqPrd.Text);
	}
	if (strcmp(emirtractHist->Leg2FltRatePmtFrqPrdMultiplier.Text, emirtract->Leg2FltRatePmtFrqPrdMultiplier.Text))
	{
		strcpy(emirtractHist->Leg2FltRatePmtFrqPrdMultiplier.Text, emirtract->Leg2FltRatePmtFrqPrdMultiplier.Text);
	}
	if (strcmp(emirtractHist->Leg2FltRateRefPrdTimePrd.Text, emirtract->Leg2FltRateRefPrdTimePrd.Text))
	{
		strcpy(emirtractHist->Leg2FltRateRefPrdTimePrd.Text, emirtract->Leg2FltRateRefPrdTimePrd.Text);
	}
	if (strcmp(emirtractHist->Leg2FltRateRefPrdMultiplier.Text, emirtract->Leg2FltRateRefPrdMultiplier.Text))
	{
		strcpy(emirtractHist->Leg2FltRateRefPrdMultiplier.Text, emirtract->Leg2FltRateRefPrdMultiplier.Text);
	}
	if (strcmp(emirtractHist->Leg2FltRateResetFrqPrd.Text, emirtract->Leg2FltRateResetFrqPrd.Text))
	{
		strcpy(emirtractHist->Leg2FltRateResetFrqPrd.Text, emirtract->Leg2FltRateResetFrqPrd.Text);
	}
	if (strcmp(emirtractHist->Leg2FltRateResetFrqMultiplier.Text, emirtract->Leg2FltRateResetFrqMultiplier.Text))
	{
		strcpy(emirtractHist->Leg2FltRateResetFrqMultiplier.Text, emirtract->Leg2FltRateResetFrqMultiplier.Text);
	}
	if (strcmp(emirtractHist->OptionType.Text, emirtract->OptionType.Text))
	{
		strcpy(emirtractHist->OptionType.Text, emirtract->OptionType.Text);
	}
	if (strcmp(emirtractHist->OptionStyle.Text, emirtract->OptionStyle.Text))
	{
		strcpy(emirtractHist->OptionStyle.Text, emirtract->OptionStyle.Text);
	}
	if (strcmp(emirtractHist->UnderlyingMaturityDate.Text, emirtract->UnderlyingMaturityDate.Text))
	{
		strcpy(emirtractHist->UnderlyingMaturityDate.Text, emirtract->UnderlyingMaturityDate.Text);
	}
	if (strcmp(emirtractHist->Seniority.Text, emirtract->Seniority.Text))
	{
		strcpy(emirtractHist->Seniority.Text, emirtract->Seniority.Text);
	}
	if (strcmp(emirtractHist->ReferenceEntity.Text, emirtract->ReferenceEntity.Text))
	{
		strcpy(emirtractHist->ReferenceEntity.Text, emirtract->ReferenceEntity.Text);
	}
	if (strcmp(emirtractHist->IndexFactor.Text, emirtract->IndexFactor.Text))
	{
		strcpy(emirtractHist->IndexFactor.Text, emirtract->IndexFactor.Text);
	}
	if (strcmp(emirtractHist->Tranche.Text, emirtract->Tranche.Text))
	{
		strcpy(emirtractHist->Tranche.Text, emirtract->Tranche.Text);
	}
	if (strcmp(emirtractHist->ProcessDate.Text, emirtract->ProcessDate.Text))
		strcpy(emirtractHist->ProcessDate.Text, emirtract->ProcessDate.Text);
	if (strcmp(emirtractHist->PriorUTI.Text, emirtract->PriorUTI.Text))
		strcpy(emirtractHist->PriorUTI.Text, emirtract->PriorUTI.Text);
	if (strcmp(emirtractHist->SubPositionUTI.Text, emirtract->SubPositionUTI.Text))
		strcpy(emirtractHist->SubPositionUTI.Text, emirtract->SubPositionUTI.Text);
	if (strcmp(emirtractHist->DerivativeCryptoAssets.Text, emirtract->DerivativeCryptoAssets.Text))
		strcpy(emirtractHist->DerivativeCryptoAssets.Text, emirtract->DerivativeCryptoAssets.Text);
	if (strcmp(emirtractHist->UnderlyingIdx.Text, emirtract->UnderlyingIdx.Text))
		strcpy(emirtractHist->UnderlyingIdx.Text, emirtract->UnderlyingIdx.Text);
	if (strcmp(emirtractHist->UnderlyingIdxName.Text, emirtract->UnderlyingIdxName.Text))
		strcpy(emirtractHist->UnderlyingIdxName.Text, emirtract->UnderlyingIdxName.Text);
	if (strcmp(emirtractHist->CustomBasketCd.Text, emirtract->CustomBasketCd.Text))
		strcpy(emirtractHist->CustomBasketCd.Text, emirtract->CustomBasketCd.Text);
	if (strcmp(emirtractHist->BasketConstituentsID.Text, emirtract->BasketConstituentsID.Text))
		strcpy(emirtractHist->BasketConstituentsID.Text, emirtract->BasketConstituentsID.Text);
	if (strcmp(emirtractHist->SettlementCcy1.Text, emirtract->SettlementCcy1.Text))
		strcpy(emirtractHist->SettlementCcy1.Text, emirtract->SettlementCcy1.Text);
	if (strcmp(emirtractHist->SettlementCcy2.Text, emirtract->SettlementCcy2.Text))
		strcpy(emirtractHist->SettlementCcy2.Text, emirtract->SettlementCcy2.Text);
	if (strcmp(emirtractHist->ValuationAmount.Text, emirtract->ValuationAmount.Text))
		strcpy(emirtractHist->ValuationAmount.Text, emirtract->ValuationAmount.Text);
	if (strcmp(emirtractHist->ValuationCcy.Text, emirtract->ValuationCcy.Text))
		strcpy(emirtractHist->ValuationCcy.Text, emirtract->ValuationCcy.Text);
	if (strcmp(emirtractHist->ValuationMthd.Text, emirtract->ValuationMthd.Text))
		strcpy(emirtractHist->ValuationMthd.Text, emirtract->ValuationMthd.Text);
	if (emirtractHist->Delta == emirtract->Delta)
		emirtractHist->Delta = emirtract->Delta;
	if (strcmp(emirtractHist->PriceUnadjustedEffDate.Text, emirtract->PriceUnadjustedEffDate.Text))
		strcpy(emirtractHist->PriceUnadjustedEffDate.Text, emirtract->PriceUnadjustedEffDate.Text);
	if (strcmp(emirtractHist->PriceUnadjustedEndDate.Text, emirtract->PriceUnadjustedEndDate.Text))
		strcpy(emirtractHist->PriceUnadjustedEndDate.Text, emirtract->PriceUnadjustedEndDate.Text);
	if (strcmp(emirtractHist->PriceInUnAdjEffAndEndDate.Text, emirtract->PriceInUnAdjEffAndEndDate.Text))
		strcpy(emirtractHist->PriceInUnAdjEffAndEndDate.Text, emirtract->PriceInUnAdjEffAndEndDate.Text);
	if (strcmp(emirtractHist->PackageTransactionPrice.Text, emirtract->PackageTransactionPrice.Text))
		strcpy(emirtractHist->PackageTransactionPrice.Text, emirtract->PackageTransactionPrice.Text);
	if (strcmp(emirtractHist->PackageTransactionPriceCcy.Text, emirtract->PackageTransactionPriceCcy.Text))
		strcpy(emirtractHist->PackageTransactionPriceCcy.Text, emirtract->PackageTransactionPriceCcy.Text);
	if (strcmp(emirtractHist->OtherPaymentType.Text, emirtract->OtherPaymentType.Text))
		strcpy(emirtractHist->OtherPaymentType.Text, emirtract->OtherPaymentType.Text);
	if (strcmp(emirtractHist->OtherPaymentAmount.Text, emirtract->OtherPaymentAmount.Text))
		strcpy(emirtractHist->OtherPaymentAmount.Text, emirtract->OtherPaymentAmount.Text);
	if (strcmp(emirtractHist->OtherPaymentCcy.Text, emirtract->OtherPaymentCcy.Text))
		strcpy(emirtractHist->OtherPaymentCcy.Text, emirtract->OtherPaymentCcy.Text);
	if (strcmp(emirtractHist->OtherPaymentDate.Text, emirtract->OtherPaymentDate.Text))
		strcpy(emirtractHist->OtherPaymentDate.Text, emirtract->OtherPaymentDate.Text);
	if (strcmp(emirtractHist->OtherPaymentPayer.Text, emirtract->OtherPaymentPayer.Text))
		strcpy(emirtractHist->OtherPaymentPayer.Text, emirtract->OtherPaymentPayer.Text);
	if (strcmp(emirtractHist->OtherPaymentreceiver.Text, emirtract->OtherPaymentreceiver.Text))
		strcpy(emirtractHist->OtherPaymentreceiver.Text, emirtract->OtherPaymentreceiver.Text);
	if (strcmp(emirtractHist->Leg1Spread.Text, emirtract->Leg1Spread.Text))
		strcpy(emirtractHist->Leg1Spread.Text, emirtract->Leg1Spread.Text);
	if (strcmp(emirtractHist->Leg1SpreadCcy.Text, emirtract->Leg1SpreadCcy.Text))
		strcpy(emirtractHist->Leg1SpreadCcy.Text, emirtract->Leg1SpreadCcy.Text);
	if (strcmp(emirtractHist->StrikePrice.Text, emirtract->StrikePrice.Text))
		strcpy(emirtractHist->StrikePrice.Text, emirtract->StrikePrice.Text);
	if (strcmp(emirtractHist->StrikePriceEffectiveDate.Text, emirtract->StrikePriceEffectiveDate.Text))
		strcpy(emirtractHist->StrikePriceEffectiveDate.Text, emirtract->StrikePriceEffectiveDate.Text);
	if (strcmp(emirtractHist->StrikePriceEndDate.Text, emirtract->StrikePriceEndDate.Text))
		strcpy(emirtractHist->StrikePriceEndDate.Text, emirtract->StrikePriceEndDate.Text);
	if (strcmp(emirtractHist->StrikePriceAssociatedEffDate.Text, emirtract->StrikePriceAssociatedEffDate.Text))
		strcpy(emirtractHist->StrikePriceAssociatedEffDate.Text, emirtract->StrikePriceAssociatedEffDate.Text);
	if (strcmp(emirtractHist->StrikePriceCcy.Text, emirtract->StrikePriceCcy.Text))
		strcpy(emirtractHist->StrikePriceCcy.Text, emirtract->StrikePriceCcy.Text);
	if (strcmp(emirtractHist->OptionPremiumAmount.Text, emirtract->OptionPremiumAmount.Text))
		strcpy(emirtractHist->OptionPremiumAmount.Text, emirtract->OptionPremiumAmount.Text);
	if (strcmp(emirtractHist->OptionPremiumCcy.Text, emirtract->OptionPremiumCcy.Text))
		strcpy(emirtractHist->OptionPremiumCcy.Text, emirtract->OptionPremiumCcy.Text);
	if (strcmp(emirtractHist->OptionPremiumPaymentDate.Text, emirtract->OptionPremiumPaymentDate.Text))
		strcpy(emirtractHist->OptionPremiumPaymentDate.Text, emirtract->OptionPremiumPaymentDate.Text);
	if (strcmp(emirtractHist->CDSIdxAttachmenPoint.Text, emirtract->CDSIdxAttachmenPoint.Text))
		strcpy(emirtractHist->CDSIdxAttachmenPoint.Text, emirtract->CDSIdxAttachmenPoint.Text);
	if (strcmp(emirtractHist->CDSIdxDetachmenPoint.Text, emirtract->CDSIdxDetachmenPoint.Text))
		strcpy(emirtractHist->CDSIdxDetachmenPoint.Text, emirtract->CDSIdxDetachmenPoint.Text);
	if (strcmp(emirtractHist->SubProduct.Text, emirtract->SubProduct.Text))
		strcpy(emirtractHist->SubProduct.Text, emirtract->SubProduct.Text);
	if (strcmp(emirtractHist->Series.Text, emirtract->Series.Text))
		strcpy(emirtractHist->Series.Text, emirtract->Series.Text);
	if (emirtractHist->cVersion == emirtract->cVersion)
		emirtractHist->cVersion = emirtract->cVersion;
	if (strcmp(emirtractHist->Leg2Spread.Text, emirtract->Leg2Spread.Text))
		strcpy(emirtractHist->Leg2Spread.Text, emirtract->Leg2Spread.Text);
	if (strcmp(emirtractHist->Leg2SpreadCcy.Text, emirtract->Leg2SpreadCcy.Text))
		strcpy(emirtractHist->Leg2SpreadCcy.Text, emirtract->Leg2SpreadCcy.Text);
	if (strcmp(emirtractHist->PackageTransactionSpread.Text, emirtract->PackageTransactionSpread.Text))
		strcpy(emirtractHist->PackageTransactionSpread.Text, emirtract->PackageTransactionSpread.Text);
	if (strcmp(emirtractHist->PackageTransactionSpreadCcy.Text, emirtract->PackageTransactionSpreadCcy.Text))
		strcpy(emirtractHist->PackageTransactionSpreadCcy.Text, emirtract->PackageTransactionSpreadCcy.Text);
	if (strcmp(emirtractHist->ExchangeRate1.Text, emirtract->ExchangeRate1.Text))
		strcpy(emirtractHist->ExchangeRate1.Text, emirtract->ExchangeRate1.Text);
	if (strcmp(emirtractHist->ForwardExchangeRate.Text, emirtract->ForwardExchangeRate.Text))
		strcpy(emirtractHist->ForwardExchangeRate.Text, emirtract->ForwardExchangeRate.Text);
	if (strcmp(emirtractHist->ExchangeRateBasis.Text, emirtract->ExchangeRateBasis.Text))
		strcpy(emirtractHist->ExchangeRateBasis.Text, emirtract->ExchangeRateBasis.Text);
	if (strcmp(emirtractHist->BaseProduct.Text, emirtract->BaseProduct.Text))
		strcpy(emirtractHist->BaseProduct.Text, emirtract->BaseProduct.Text);
	if (strcmp(emirtractHist->FurtherSubProduct.Text, emirtract->FurtherSubProduct.Text))
		strcpy(emirtractHist->FurtherSubProduct.Text, emirtract->FurtherSubProduct.Text);
	if (strcmp(emirtractHist->DeliveryPoint.Text, emirtract->DeliveryPoint.Text))
		strcpy(emirtractHist->DeliveryPoint.Text, emirtract->DeliveryPoint.Text);
	if (strcmp(emirtractHist->InterConnectionPoint.Text, emirtract->InterConnectionPoint.Text))
		strcpy(emirtractHist->InterConnectionPoint.Text, emirtract->InterConnectionPoint.Text);
	if (strcmp(emirtractHist->LoadType.Text, emirtract->LoadType.Text))
		strcpy(emirtractHist->LoadType.Text, emirtract->LoadType.Text);
	if (strcmp(emirtractHist->DeliveryIntervalStartTime.Text, emirtract->DeliveryIntervalStartTime.Text))
		strcpy(emirtractHist->DeliveryIntervalStartTime.Text, emirtract->DeliveryIntervalStartTime.Text);
	if (strcmp(emirtractHist->DeliveryIntervalEndTime.Text, emirtract->DeliveryIntervalEndTime.Text))
		strcpy(emirtractHist->DeliveryIntervalEndTime.Text, emirtract->DeliveryIntervalEndTime.Text);
	if (strcmp(emirtractHist->DeliveryStartDate.Text, emirtract->DeliveryStartDate.Text))
		strcpy(emirtractHist->DeliveryStartDate.Text, emirtract->DeliveryStartDate.Text);
	if (strcmp(emirtractHist->DeliveryEndDate.Text, emirtract->DeliveryEndDate.Text))
		strcpy(emirtractHist->DeliveryEndDate.Text, emirtract->DeliveryEndDate.Text);
	if (emirtractHist->cDuration == emirtract->cDuration)
		emirtractHist->cDuration = emirtract->cDuration;
	if (emirtractHist->WeekDays == emirtract->WeekDays)
		emirtractHist->WeekDays = emirtract->WeekDays;
	if (strcmp(emirtractHist->DeliveryCapacity.Text, emirtract->DeliveryCapacity.Text))
		strcpy(emirtractHist->DeliveryCapacity.Text, emirtract->DeliveryCapacity.Text);
	if (strcmp(emirtractHist->QuantityUnit.Text, emirtract->QuantityUnit.Text))
		strcpy(emirtractHist->QuantityUnit.Text, emirtract->QuantityUnit.Text);
	if (strcmp(emirtractHist->PriceTimeintervalQuantity.Text, emirtract->PriceTimeintervalQuantity.Text))
		strcpy(emirtractHist->PriceTimeintervalQuantity.Text, emirtract->PriceTimeintervalQuantity.Text);
	if (strcmp(emirtractHist->PriceCcyTimeIntervalQuantity.Text, emirtract->PriceCcyTimeIntervalQuantity.Text))
		strcpy(emirtractHist->PriceCcyTimeIntervalQuantity.Text, emirtract->PriceCcyTimeIntervalQuantity.Text);
	if (strcmp(emirtractHist->EventDate.Text, emirtract->EventDate.Text))
		strcpy(emirtractHist->EventDate.Text, emirtract->EventDate.Text);
	if (strcmp(emirtractHist->EventType.Text, emirtract->EventType.Text))
		strcpy(emirtractHist->EventType.Text, emirtract->EventType.Text);

	return true;
}

EmirTrade *EmirReport::InitEmirTrade(const sTRADE *trade)
{
	if (trade->Base.TradeType == sTT_EXOTIC && trade->Exotic.Assets.Data[0].SubType == sST_ETRS)
	{
		EmirEquity *equityTrade = new EmirEquity(trade, Date, AppDate, Time, AppTime, Debug);
		return equityTrade;
	}
	else if (trade->Base.TradeType == sTT_FRA)
	{
		EmirFRA *fraTrade = new EmirFRA(trade, Date, AppDate, Time, AppTime, Debug);
		return fraTrade;
	}
	else if (trade->Base.TradeType == sTT_SWAP)
	{
		EmirSwap *irTrade = new EmirSwap(trade, Date, AppDate, Time, AppTime, Debug);
		return irTrade;
	}
	else if (trade->Base.TradeType == sTT_SWAPTION)
	{
		EmirSwaption *irTrade = new EmirSwaption(trade, Date, AppDate, Time, AppTime, Debug);
		return irTrade;
	}
	else if (trade->Base.TradeType == sTT_EXOTIC)
	{
		EmirExotic *irTrade = new EmirExotic(trade, Date, AppDate, Time, AppTime, Debug);
		return irTrade;
	}
	else if (trade->Base.TradeType == sTT_IRG)
	{
		EmirIRG *irTrade = new EmirIRG(trade, Date, AppDate, Time, AppTime, Debug);
		return irTrade;
	}
	else if (trade->Base.TradeType == sTT_FXFWD || trade->Base.TradeType == sTT_FXSWAP)
	{
		EmirFXFwdSwap *fxTrade = new EmirFXFwdSwap(trade, Date, AppDate, Time, AppTime, Debug);
		return fxTrade;
	}
	else if (trade->Base.TradeType == sTT_FXOPTION)
	{
		EmirFXOpt *fxTrade = new EmirFXOpt(trade, Date, AppDate, Time, AppTime, Debug);
		return fxTrade;
	}
	else if (trade->Base.TradeType == sTT_MUST)
	{
		EmirMust *mustTrade = new EmirMust(trade, Date, AppDate, Time, AppTime, Debug);
		return mustTrade;
	}

	return NULL;
}

/**/
void EmirReport::printErrXML(ChROME::EntityWrapper<cEMRTRACT> &emirTrdActivityerr, ostream *ptrstream)
{
	ChROME::EntityWrapper<cEMRTRACT> emirTrdActivity2exit("cEMRTRACT");
	strcpy(emirTrdActivity2exit->ActionType.Text, "Err");

	xml_document doc;
	string srefit2modelpath = getenv("STKROOT");
	srefit2modelpath.append("/etc/EMIR_XMLModelFiles/refit2modele.xml");
	xml_parse_result result = doc.load_file(srefit2modelpath.c_str());
	if (!result)
		return;

	xml_node errxmlnode = doc.child("root");

	xml_node CmonTradDatanode = errxmlnode.first_element_by_path("CmonTradData");
	xml_node CtrPtySpcfcDatanode = errxmlnode.first_element_by_path("CtrPtySpcfcData");

	xpath_node xpathNode = CmonTradDatanode.select_single_node("TxData/TxId"); //../CmonTradData/TxData/TxId/UnqTxIdr
	if (xpathNode && !ChROME::trim(emirTrdActivityerr->UTI.Text).empty())
	{
		xml_node selectedNode = xpathNode.node();
		selectedNode.child("UnqTxIdr").text().set(emirTrdActivityerr->UTI.Text);
		strcpy(emirTrdActivity2exit->UTI.Text, emirTrdActivityerr->UTI.Text);
	}
	xpathNode = CmonTradDatanode.select_single_node("TxData/TxId/Prtry"); //../CmonTradData/TxData/TxId/Prtry
	if (xpathNode && !ChROME::trim(emirTrdActivityerr->UTI.Text).empty())
	{
		xml_node selectedNode = xpathNode.node();
		selectedNode.child("Prtry").text().set(emirTrdActivityerr->UTI.Text);
	}

	xpathNode = CmonTradDatanode.select_single_node("TxData/PrrTxId"); //../CmonTradData/TxData/PrrTxId/UnqTxIdr
	if (xpathNode && !ChROME::trim(emirTrdActivityerr->PriorUTI.Text).empty())
	{
		xml_node selectedNode = xpathNode.node();
		selectedNode.child("UnqTxIdr").text().set(emirTrdActivityerr->PriorUTI.Text);
		strcpy(emirTrdActivity2exit->PriorUTI.Text, emirTrdActivityerr->PriorUTI.Text);
	}
	xpathNode = CmonTradDatanode.select_single_node("TxData/PrrTxId/Prtry");
	if (xpathNode && !ChROME::trim(emirTrdActivityerr->PriorUTI.Text).empty())
	{
		xml_node selectedNode = xpathNode.node();
		selectedNode.child("Prtry").text().set(emirTrdActivityerr->PriorUTI.Text);
		strcpy(emirTrdActivity2exit->PriorUTI.Text, emirTrdActivityerr->PriorUTI.Text);
	}

	string sReportingTimestamp = ChROME::UTCTimeStamp(AppDate, AppTime, true);
	CtrPtySpcfcDatanode.child("RptgTmStmp").text().set(sReportingTimestamp.c_str()); // CtrPtySpcfcData/RptgTmStmp
	strcpy(emirTrdActivity2exit->ReportingTimestamp.Text, sReportingTimestamp.c_str());

	xpathNode = CtrPtySpcfcDatanode.select_single_node("CtrPty/SubmitgAgt"); //../CtrPtySpcfcData/CtrPty/SubmitgAgt/LEI
	if (xpathNode && !ChROME::trim(emirTrdActivityerr->ReportSubmittingEntityID.Text).empty())
	{
		xml_node selectedNode = xpathNode.node();
		selectedNode.child("LEI").text().set(emirTrdActivityerr->ReportSubmittingEntityID.Text);
		strcpy(emirTrdActivity2exit->ReportSubmittingEntityID.Text, emirTrdActivityerr->ReportSubmittingEntityID.Text);
	}

	xpathNode = CtrPtySpcfcDatanode.select_single_node("CtrPty/RptgCtrPty/Id/Lgl/Id"); //../CtrPtySpcfcData/CtrPty/RptgCtrPty/Id/Lgl/Id/LEI
	if (xpathNode)
	{
		xml_node selectedNode = xpathNode.node();
		selectedNode.child("LEI").text().set(emirTrdActivityerr->Counterparty1.Text);
		strcpy(emirTrdActivity2exit->Counterparty1.Text, emirTrdActivityerr->Counterparty1.Text);
	}

	// string sTradeParty2Prefix = GetTradeParty2Prefix();
	xpathNode = CtrPtySpcfcDatanode.select_single_node("CtrPty/OthrCtrPty/IdTp/Lgl/Id"); //../CtrPtySpcfcData/CtrPty/OthrCtrPty/IdTp/Lgl/Id/LEI
																						 // if (xpathNode && (sTradeParty2Prefix == "LEI")) {
	xml_node selectedNode = xpathNode.node();
	selectedNode.child("LEI").text().set(emirTrdActivityerr->Counterparty2.Text);
	strcpy(emirTrdActivity2exit->Counterparty2.Text, emirTrdActivityerr->Counterparty2.Text);
	/*} else {
		xpathNode = CtrPtySpcfcDatanode.select_single_node("CtrPty/OthrCtrPty/IdTp/Ntrl");//../CtrPtySpcfcData/CtrPty/OthrCtrPty/IdTp/Ntrl/Ctry
		if (xpathNode) {
			xml_node selectedNode = xpathNode.node();
			selectedNode.child("Ctry").text().set(emirTrdActivityerr->Counterparty2Country.Text);
		}
	}*/

	xpathNode = CtrPtySpcfcDatanode.select_single_node("CtrPty/NttyRspnsblForRpt"); //../CtrPtySpcfcData/CtrPty/NttyRspnsblForRpt/LEI
	if (xpathNode && !ChROME::trim(emirTrdActivityerr->ReportingResponsibleEntity.Text).empty())
	{
		xml_node selectedNode = xpathNode.node();
		selectedNode.child("LEI").text().set(emirTrdActivityerr->ReportingResponsibleEntity.Text);
		strcpy(emirTrdActivity2exit->ReportingResponsibleEntity.Text, emirTrdActivityerr->ReportingResponsibleEntity.Text);
	}

	xpathNode = CmonTradDatanode.select_single_node("TxData/DerivEvt/TmStmp"); //../CmonTradData/TxData/DerivEvt/TmStmp/Dt
	if (xpathNode)
	{
		xml_node selectedNode = xpathNode.node();
		string sEventDate = sReportingTimestamp.substr(0, 10);
		selectedNode.child("Dt").text().set(sEventDate.c_str());
		strcpy(emirTrdActivity2exit->EventDate.Text, sEventDate.c_str());
	}

	errxmlnode.child("Lvl").text().set(emirTrdActivityerr->cLevel.Text);
	strcpy(emirTrdActivity2exit->cLevel.Text, emirTrdActivityerr->cLevel.Text);

	CopyDiff(emirTrdActivityerr, emirTrdActivity2exit);
	strcpy(emirTrdActivityerr->ActionType.Text, "Err");

	errxmlnode.set_name("Err");

	removenode(errxmlnode);
	doc.save(*ptrstream);
}

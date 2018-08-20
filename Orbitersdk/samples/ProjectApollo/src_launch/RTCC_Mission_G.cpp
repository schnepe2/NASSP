/****************************************************************************
This file is part of Project Apollo - NASSP
Copyright 2018

RTCC Calculations for Mission G

Project Apollo is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

Project Apollo is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Project Apollo; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

See http://nassp.sourceforge.net/license/ for more details.

**************************************************************************/

#include "Orbitersdk.h"
#include "soundlib.h"
#include "apolloguidance.h"
#include "saturn.h"
#include "saturnv.h"
#include "LVDC.h"
#include "iu.h"
#include "LEM.h"
#include "../src_rtccmfd/OrbMech.h"
#include "mcc.h"
#include "rtcc.h"

bool RTCC::CalculationMTP_G(int fcn, LPVOID &pad, char * upString, char * upDesc, char * upMessage)
{
	char uplinkdata[1024 * 3];
	bool preliminary = false;
	bool scrubbed = false;

	//Mission Constants
	double AGCEpoch = 40586.767239;
	int LGCREFSAddrOffs = -2;
	int LGCDeltaVAddr = 3433;

	switch (fcn) {
	case 1: //GENERIC CSM STATE VECTOR UPDATE
	{
		SV sv;
		double GETbase;
		char buffer1[1000];

		sv = StateVectorCalc(calcParams.src); //State vector for uplink
		GETbase = calcParams.TEPHEM;

		AGCStateVectorUpdate(buffer1, sv, true, AGCEpoch, GETbase);

		sprintf(uplinkdata, "%s", buffer1);
		if (upString != NULL) {
			// give to mcc
			strncpy(upString, uplinkdata, 1024 * 3);
			sprintf(upDesc, "CSM state vector");
		}
	}
	break;
	case 10: //GROUND LIFTOFF TIME UPDATE
	{
		double TEPHEM0, tephem_scal;

		Saturn *cm = (Saturn *)calcParams.src;

		//Get TEPHEM
		TEPHEM0 = 40403.;
		tephem_scal = GetTEPHEMFromAGC(&cm->agc.vagc);
		calcParams.TEPHEM = (tephem_scal / 8640000.) + TEPHEM0;
	}
	break;
	case 11: //TLI SIMULATION
	{
		SaturnV *SatV = (SaturnV*)calcParams.src;
		LVDCSV *lvdc = (LVDCSV*)SatV->iu->lvdc;

		LVDCTLIparam tliparam;

		tliparam.alpha_TS = lvdc->alpha_TS;
		tliparam.Azimuth = lvdc->Azimuth;
		tliparam.beta = lvdc->beta;
		tliparam.cos_sigma = lvdc->cos_sigma;
		tliparam.C_3 = lvdc->C_3;
		tliparam.e_N = lvdc->e_N;
		tliparam.f = lvdc->f;
		tliparam.mu = lvdc->mu;
		tliparam.MX_A = lvdc->MX_A;
		tliparam.omega_E = lvdc->omega_E;
		tliparam.phi_L = lvdc->PHI;
		tliparam.R_N = lvdc->R_N;
		tliparam.T_2R = lvdc->T_2R;
		tliparam.TargetVector = lvdc->TargetVector;
		tliparam.TB5 = lvdc->TB5;
		tliparam.theta_EO = lvdc->theta_EO;
		tliparam.t_D = lvdc->t_D;
		tliparam.T_L = lvdc->T_L;
		tliparam.T_RG = lvdc->T_RG;
		tliparam.T_ST = lvdc->T_ST;
		tliparam.Tt_3R = lvdc->Tt_3R;

		LVDCTLIPredict(tliparam, calcParams.src, getGETBase(), DeltaV_LVLH, TimeofIgnition, calcParams.R_TLI, calcParams.V_TLI, calcParams.TLI);
	}
	break;
	case 12: //TLI+90 MANEUVER PAD
	{
		EntryOpt entopt;
		EntryResults res;
		AP11ManPADOpt opt;
		double GETbase, TLIBase, TIG, CSMmass;
		SV sv, sv1;
		char buffer1[1000];

		AP11MNV * form = (AP11MNV *)pad;

		GETbase = calcParams.TEPHEM;

		TLIBase = TimeofIgnition;
		TIG = TLIBase + 90.0*60.0;

		sv = StateVectorCalc(calcParams.src); //State vector for uplink

		CSMmass = 28862.0;

		sv1.mass = CSMmass;
		sv1.gravref = AGCGravityRef(calcParams.src);
		sv1.MJD = GETbase + calcParams.TLI / 24.0 / 3600.0;
		sv1.R = calcParams.R_TLI;
		sv1.V = calcParams.V_TLI;

		entopt.entrylongmanual = false;
		entopt.GETbase = GETbase;
		entopt.impulsive = RTCC_NONIMPULSIVE;
		//Code for Atlantic Ocean Line
		entopt.lng = 2.0;
		entopt.ReA = 0;
		entopt.TIGguess = TIG;
		entopt.type = RTCC_ENTRY_ABORT;
		entopt.vessel = calcParams.src;
		entopt.useSV = true;
		entopt.RV_MCC = sv1;

		EntryTargeting(&entopt, &res); //Target Load for uplink

		opt.dV_LVLH = res.dV_LVLH;
		opt.enginetype = RTCC_ENGINETYPE_SPSDPS;
		opt.GETbase = GETbase;
		opt.HeadsUp = true;
		opt.REFSMMAT = GetREFSMMATfromAGC(&mcc->cm->agc.vagc, AGCEpoch);
		opt.TIG = res.P30TIG;
		opt.vessel = calcParams.src;
		opt.vesseltype = 0;
		opt.useSV = true;
		opt.RV_MCC = sv1;

		AP11ManeuverPAD(&opt, *form);
		form->lat = res.latitude*DEG;
		form->lng = res.longitude*DEG;
		form->RTGO = res.RTGO;
		form->VI0 = res.VIO / 0.3048;
		form->Weight = CSMmass / 0.45359237;
		form->GET05G = res.GET05G;

		sprintf(form->purpose, "TLI+90");
		sprintf(form->remarks, "No ullage, undocked");

		AGCStateVectorUpdate(buffer1, sv, true, AGCEpoch, calcParams.TEPHEM);

		sprintf(uplinkdata, "%s", buffer1);
		if (upString != NULL) {
			// give to mcc
			strncpy(upString, uplinkdata, 1024 * 3);
			sprintf(upDesc, "CSM state vector");
		}
	}
	break;
	case 13: //TLI+5 PAD
	{
		AP11BLKOpt opt;
		double CSMmass, GETbase;
		SV sv1;

		P37PAD * form = (P37PAD *)pad;

		GETbase = calcParams.TEPHEM;

		CSMmass = 28862.0;

		sv1.mass = CSMmass;
		sv1.gravref = AGCGravityRef(calcParams.src);
		sv1.MJD = GETbase + calcParams.TLI / 24.0 / 3600.0;
		sv1.R = calcParams.R_TLI;
		sv1.V = calcParams.V_TLI;

		opt.n = 1;

		opt.GETI.push_back(TimeofIgnition + 5.0*3600.0);
		opt.lng.push_back(-165.0*RAD);
		opt.useSV = true;
		opt.RV_MCC = sv1;

		AP11BlockData(&opt, *form);
	}
	break;
	case 14: //TLI PAD
	{
		TLIPADOpt opt;
		double GETbase;

		TLIPAD * form = (TLIPAD *)pad;

		GETbase = calcParams.TEPHEM;

		opt.dV_LVLH = DeltaV_LVLH;
		opt.GETbase = GETbase;
		opt.R_TLI = calcParams.R_TLI;
		opt.V_TLI = calcParams.V_TLI;
		opt.REFSMMAT = GetREFSMMATfromAGC(&mcc->cm->agc.vagc, AGCEpoch);
		opt.TIG = TimeofIgnition;
		opt.TLI = calcParams.TLI;
		opt.vessel = calcParams.src;
		opt.SeparationAttitude = _V(PI, 120.0*RAD, 40.0*RAD);
		opt.uselvdc = true;

		TLI_PAD(&opt, *form);

		sprintf(form->remarks, "TLI 10-minute abort pitch, 223�");
	}
	break;
	case 15: //TLI Evaluation
	{
		SaturnV *SatV = (SaturnV*)calcParams.src;
		LVDCSV *lvdc = (LVDCSV*)SatV->iu->lvdc;

		if (lvdc->first_op == false)
		{
			scrubbed = true;
		}
	}
	break;
	case 16: //Evasive Maneuver
	{
		AP11ManPADOpt opt;

		AP11MNV * form = (AP11MNV *)pad;

		opt.dV_LVLH = _V(5.1, 0.0, 19.0)*0.3048;
		opt.enginetype = RTCC_ENGINETYPE_SPSDPS;
		opt.GETbase = calcParams.TEPHEM;
		opt.HeadsUp = true;
		opt.REFSMMAT = GetREFSMMATfromAGC(&mcc->cm->agc.vagc, AGCEpoch);
		opt.TIG = calcParams.TLI + 1.0*3600.0 + 50.0*60.0;
		opt.vessel = calcParams.src;
		opt.vesseltype = 1;

		AP11ManeuverPAD(&opt, *form);

		sprintf(form->purpose, "Evasive");
	}
	break;
	case 17: //Block Data 1
	case 18: //Block Data 2
	{
		AP11BLKOpt opt;
		double GETbase;

		P37PAD * form = (P37PAD *)pad;

		GETbase = calcParams.TEPHEM;

		double TLIbase = calcParams.TLI - 5.0*60.0 - 20.0; //Approximate TLI ignition

		if (fcn == 17)
		{
			opt.n = 1;

			opt.GETI.push_back(TLIbase + 11.0*3600.0);
			opt.lng.push_back(-165.0*RAD);
		}
		else
		{
			opt.n = 4;

			opt.GETI.push_back(TLIbase + 25.0*3600.0);
			opt.lng.push_back(-165.0*RAD);
			opt.GETI.push_back(TLIbase + 35.0*3600.0);
			opt.lng.push_back(-165.0*RAD);
			opt.GETI.push_back(TLIbase + 44.0*3600.0);
			opt.lng.push_back(-165.0*RAD);
			opt.GETI.push_back(TLIbase + 53.0*3600.0);
			opt.lng.push_back(-165.0*RAD);
		}

		AP11BlockData(&opt, *form);
	}
	break;
	case 19: //PTC REFSMMAT
	{
		char buffer1[1000];

		MATRIX3 REFSMMAT = _M(0.866025404, -0.45872739, -0.19892002, -0.5, -0.79453916, -0.34453958, 0.0, 0.39784004, -0.91745480);

		AGCDesiredREFSMMATUpdate(buffer1, REFSMMAT, AGCEpoch, true, true);
		sprintf(uplinkdata, "%s", buffer1);

		if (upString != NULL) {
			// give to mcc
			strncpy(upString, uplinkdata, 1024 * 3);
			sprintf(upDesc, "PTC REFSMMAT");
		}
	}
	break;
	case 20: //MCC-1 EVALUATION
	case 21: //MCC-1 CALCULATION AND UPDATE
	case 22: //MCC-2 CALCULATION AND UPDATE
	{
		double GETbase, MCC1GET, MCC2GET, MCC3GET;
		SV sv;
		char manname[8];

		DeltaV_LVLH = _V(0, 0, 0);
		TimeofIgnition = 0.0;

		if (fcn == 20 || fcn == 21)
		{
			sprintf(manname, "MCC1");
		}
		else if (fcn == 22)
		{
			sprintf(manname, "MCC2");
		}

		double TLIbase = calcParams.TLI - 5.0*60.0 - 20.0; //Approximate TLI ignition

		MCC1GET = TLIbase + 9.0*3600.0;
		MCC2GET = TLIbase + 24.0*3600.0;
		MCC3GET = calcParams.LOI - 22.0*3600.0;

		sv = StateVectorCalc(calcParams.src);
		GETbase = calcParams.TEPHEM;

		MCCFRMan opt;
		TLMCCResults res;

		opt.type = 0;
		opt.lat = calcParams.PericynthionLatitude;
		opt.PeriGET = calcParams.LOI;
		opt.h_peri = 60.0 * 1852.0;
		opt.alt = calcParams.LSAlt;
		opt.azi = calcParams.LSAzi;
		opt.csmlmdocked = true;
		opt.GETbase = GETbase;

		opt.LOIh_apo = 170.0*1852.0;
		opt.LOIh_peri = 60.0*1852.0;
		opt.LSlat = calcParams.LSLat;
		opt.LSlng = calcParams.LSLng;
		opt.t_land = calcParams.TLAND;
		opt.vessel = calcParams.src;

		//Evaluate MCC-3 DV
		opt.MCCGET = MCC3GET;
		if (TranslunarMidcourseCorrectionTargetingFreeReturn(&opt, &res))
		{
			calcParams.alt_node = res.NodeAlt;
			calcParams.lat_node = res.NodeLat;
			calcParams.lng_node = res.NodeLng;
			calcParams.GET_node = res.NodeGET;
			calcParams.LOI = res.PericynthionGET;
		}

		if (length(res.dV_LVLH) < 25.0*0.3048)
		{
			scrubbed = true;
		}
		else
		{
			if (fcn == 20 || fcn == 21)
			{
				opt.MCCGET = MCC1GET;
			}
			else
			{
				opt.MCCGET = MCC2GET;
			}

			TranslunarMidcourseCorrectionTargetingFreeReturn(&opt, &res);

			//Scrub MCC-1 if DV is less than 50 ft/s
			if (fcn == 20 && length(res.dV_LVLH) < 50.0*0.3048)
			{
				scrubbed = true;
			}
			else
			{
				calcParams.alt_node = res.NodeAlt;
				calcParams.lat_node = res.NodeLat;
				calcParams.lng_node = res.NodeLng;
				calcParams.GET_node = res.NodeGET;
				calcParams.LOI = res.PericynthionGET;
				DeltaV_LVLH = res.dV_LVLH;
				TimeofIgnition = res.P30TIG;
			}
		}

		//MCC-1 Evaluation
		if (fcn == 20)
		{
			if (scrubbed)
			{
				sprintf(upMessage, "%s has been scrubbed.", manname);
			}
		}
		//MCC-1 and MCC-2 Update
		else
		{
			if (scrubbed)
			{
				char buffer1[1000];

				sprintf(upMessage, "%s has been scrubbed.", manname);
				sprintf(upDesc, "CSM state vector");

				AGCStateVectorUpdate(buffer1, sv, true, AGCEpoch, GETbase);

				sprintf(uplinkdata, "%s", buffer1);
				if (upString != NULL) {
					// give to mcc
					strncpy(upString, uplinkdata, 1024 * 3);
				}
			}
			else
			{
				char buffer1[1000];
				char buffer2[1000];
				AP11ManPADOpt manopt;

				AP11MNV * form = (AP11MNV *)pad;

				manopt.dV_LVLH = DeltaV_LVLH;
				manopt.enginetype = SPSRCSDecision(SPS_THRUST / (calcParams.src->GetMass() + calcParams.tgt->GetMass()), DeltaV_LVLH);
				manopt.GETbase = GETbase;
				manopt.HeadsUp = false;
				manopt.REFSMMAT = GetREFSMMATfromAGC(&mcc->cm->agc.vagc, AGCEpoch);
				manopt.TIG = TimeofIgnition;
				manopt.vessel = calcParams.src;
				manopt.vesseltype = 1;

				AP11ManeuverPAD(&manopt, *form);
				sprintf(form->purpose, manname);
				sprintf(form->remarks, "LM weight is %.0f.", form->LMWeight);

				AGCStateVectorUpdate(buffer1, sv, true, AGCEpoch, GETbase);
				AGCExternalDeltaVUpdate(buffer2, TimeofIgnition, DeltaV_LVLH);

				sprintf(uplinkdata, "%s%s", buffer1, buffer2);
				if (upString != NULL) {
					// give to mcc
					strncpy(upString, uplinkdata, 1024 * 3);
					sprintf(upDesc, "CSM state vector, target load");
				}
			}
		}
	}
	break;
	case 23: //Lunar Flyby PAD
	{
		RTEFlybyOpt entopt;
		EntryResults res;
		AP11ManPADOpt opt;
		SV sv;
		double GETbase;
		char buffer1[1000];

		AP11MNV * form = (AP11MNV *)pad;

		GETbase = calcParams.TEPHEM;

		sv = StateVectorCalc(calcParams.src); //State vector for uplink

		entopt.EntryLng = -165.0*RAD;
		entopt.GETbase = GETbase;
		entopt.returnspeed = 1;
		entopt.FlybyType = 0;
		entopt.TIGguess = calcParams.LOI - 5.0*3600.0;
		entopt.vessel = calcParams.src;

		RTEFlybyTargeting(&entopt, &res);

		SV sv_peri = FindPericynthion(res.sv_postburn);
		double h_peri = length(sv_peri.R) - oapiGetSize(oapiGetObjectByName("Moon"));

		opt.alt = calcParams.LSAlt;
		opt.dV_LVLH = res.dV_LVLH;
		opt.enginetype = SPSRCSDecision(SPS_THRUST / (calcParams.src->GetMass() + calcParams.tgt->GetMass()), res.dV_LVLH);
		opt.GETbase = GETbase;
		opt.HeadsUp = false;
		opt.REFSMMAT = GetREFSMMATfromAGC(&mcc->cm->agc.vagc, AGCEpoch);
		opt.TIG = res.P30TIG;
		opt.vessel = calcParams.src;
		opt.vesseltype = 1;

		AP11ManeuverPAD(&opt, *form);

		sprintf(form->purpose, "Flyby");
		sprintf(form->remarks, "Height of pericynthion is %.0f NM", h_peri / 1852.0);
		form->lat = res.latitude*DEG;
		form->lng = res.longitude*DEG;
		form->RTGO = res.RTGO;
		form->VI0 = res.VIO / 0.3048;
		form->GET05G = res.GET05G;

		//Save parameters for further use
		SplashLatitude = res.latitude;
		SplashLongitude = res.longitude;
		calcParams.TEI = res.P30TIG;
		calcParams.EI = res.GET400K;

		AGCStateVectorUpdate(buffer1, sv, true, AGCEpoch, GETbase);

		sprintf(uplinkdata, "%s", buffer1);
		if (upString != NULL) {
			// give to mcc
			strncpy(upString, uplinkdata, 1024 * 3);
			sprintf(upDesc, "CSM state vector");
		}
	}
	break;
	case 24: //MCC-3
	{
		AP11ManPADOpt manopt;
		LOIMan loiopt;
		MCCNodeMan opt;
		VECTOR3 dV_LVLH, dV_LOI;
		SV sv, sv_peri, sv_node, sv_postLOI;
		double GETbase, MCCGET, P30TIG, r_M, TIG_LOI, h_peri, h_node;

		AP11MNV * form = (AP11MNV *)pad;

		GETbase = calcParams.TEPHEM;
		r_M = oapiGetSize(oapiGetObjectByName("Moon"));
		sv = StateVectorCalc(calcParams.src); //State vector for uplink

		loiopt.alt = calcParams.LSAlt;
		loiopt.azi = calcParams.LSAzi;
		loiopt.csmlmdocked = true;
		loiopt.GETbase = GETbase;
		loiopt.h_apo = 170.0*1852.0;
		loiopt.h_peri = 60.0*1852.0;
		loiopt.impulsive = RTCC_IMPULSIVE;
		loiopt.lat = calcParams.LSLat;
		loiopt.lng = calcParams.LSLng;
		loiopt.t_land = calcParams.TLAND;
		loiopt.vessel = calcParams.src;

		LOITargeting(&loiopt, dV_LOI, TIG_LOI, sv_node, sv_postLOI);

		sv_peri = FindPericynthion(sv);

		h_peri = length(sv_peri.R) - r_M;
		h_node = length(sv_node.R) - r_M;

		//Maneuver execution criteria
		if (h_peri > 50.0*1852.0 && h_peri < 70.0*1852.0)
		{
			if (h_node > 50.0*1852.0 && h_node < 75.0*1852.0)
			{
				scrubbed = true;
			}
		}

		if (scrubbed)
		{
			char buffer1[1000];

			sprintf(upMessage, "MCC-3 has been scrubbed");
			sprintf(upDesc, "CSM state vector");

			AGCStateVectorUpdate(buffer1, sv, true, AGCEpoch, GETbase);

			sprintf(uplinkdata, "%s", buffer1);
			if (upString != NULL) {
				// give to mcc
				strncpy(upString, uplinkdata, 1024 * 3);
			}
		}
		else
		{
			char buffer1[1000];
			char buffer2[1000];

			MCCGET = calcParams.LOI - 22.0*3600.0;

			opt.lat = calcParams.lat_node;
			opt.lng = calcParams.lng_node;
			opt.NodeGET = calcParams.GET_node;
			opt.h_node = calcParams.alt_node;
			opt.csmlmdocked = true;
			opt.GETbase = GETbase;
			opt.MCCGET = MCCGET;
			opt.vessel = calcParams.src;

			TranslunarMidcourseCorrectionTargetingNodal(&opt, dV_LVLH, P30TIG);

			manopt.dV_LVLH = dV_LVLH;
			manopt.enginetype = SPSRCSDecision(SPS_THRUST / calcParams.src->GetMass(), dV_LVLH);
			manopt.GETbase = GETbase;
			manopt.HeadsUp = false;
			manopt.REFSMMAT = GetREFSMMATfromAGC(&mcc->cm->agc.vagc, AGCEpoch);
			manopt.TIG = P30TIG;
			manopt.vessel = calcParams.src;
			manopt.vesseltype = 1;

			AP11ManeuverPAD(&manopt, *form);
			sprintf(form->purpose, "MCC-3");

			AGCStateVectorUpdate(buffer1, sv, true, AGCEpoch, GETbase);
			AGCExternalDeltaVUpdate(buffer2, P30TIG, dV_LVLH);

			sprintf(uplinkdata, "%s%s", buffer1, buffer2);
			if (upString != NULL) {
				// give to mcc
				strncpy(upString, uplinkdata, 1024 * 3);
				sprintf(upDesc, "CSM state vector, target load");
			}
		}
	}
	break;
	case 25: //MCC-4
	{
		LOIMan loiopt;
		REFSMMATOpt refsopt;
		VECTOR3 dV_LVLH, dV_LOI;
		SV sv, sv_peri, sv_node, sv_postLOI;
		MATRIX3 REFSMMAT;
		double GETbase, MCCGET, P30TIG, r_M, TIG_LOI, h_peri, h_node;

		AP11MNV * form = (AP11MNV *)pad;

		GETbase = calcParams.TEPHEM;
		r_M = oapiGetSize(oapiGetObjectByName("Moon"));
		sv = StateVectorCalc(calcParams.src); //State vector for uplink

		loiopt.alt = calcParams.LSAlt;
		loiopt.azi = calcParams.LSAzi;
		loiopt.csmlmdocked = true;
		loiopt.GETbase = GETbase;
		loiopt.h_apo = 170.0*1852.0;
		loiopt.h_peri = 60.0*1852.0;
		loiopt.impulsive = RTCC_IMPULSIVE;
		loiopt.lat = calcParams.LSLat;
		loiopt.lng = calcParams.LSLng;
		loiopt.t_land = calcParams.TLAND;
		loiopt.vessel = calcParams.src;

		LOITargeting(&loiopt, dV_LOI, TIG_LOI, sv_node, sv_postLOI);

		sv_peri = FindPericynthion(sv);

		h_peri = length(sv_peri.R) - r_M;
		h_node = length(sv_node.R) - r_M;

		//Maneuver execution criteria
		if (h_peri > 50.0*1852.0 && h_peri < 70.0*1852.0)
		{
			if (h_node > 50.0*1852.0 && h_node < 75.0*1852.0)
			{
				scrubbed = true;
			}
		}

		//REFSMMAT calculation
		refsopt.GETbase = GETbase;
		refsopt.LSAzi = calcParams.LSAzi;
		refsopt.LSLat = calcParams.LSLat;
		refsopt.LSLng = calcParams.LSLng;
		refsopt.REFSMMATopt = 8;
		refsopt.REFSMMATTime = calcParams.TLAND;

		REFSMMAT = REFSMMATCalc(&refsopt);
		calcParams.StoredREFSMMAT = REFSMMAT;

		if (scrubbed)
		{
			char buffer1[1000];
			char buffer2[1000];

			sprintf(upMessage, "MCC-4 has been scrubbed");
			sprintf(upDesc, "CSM state vector, Landing Site REFSMMAT");

			AGCStateVectorUpdate(buffer1, sv, true, AGCEpoch, GETbase);
			AGCDesiredREFSMMATUpdate(buffer2, REFSMMAT, AGCEpoch);

			sprintf(uplinkdata, "%s%s", buffer1, buffer2);
			if (upString != NULL) {
				// give to mcc
				strncpy(upString, uplinkdata, 1024 * 3);
			}
		}
		else
		{
			AP11ManPADOpt manopt;
			MCCNodeMan opt;
			char buffer1[1000];
			char buffer2[1000];
			char buffer3[1000];

			MCCGET = calcParams.LOI - 5.0*3600.0;

			opt.lat = calcParams.lat_node;
			opt.lng = calcParams.lng_node;
			opt.NodeGET = calcParams.GET_node;
			opt.h_node = calcParams.alt_node;
			opt.csmlmdocked = true;
			opt.GETbase = GETbase;
			opt.MCCGET = MCCGET;
			opt.vessel = calcParams.src;

			TranslunarMidcourseCorrectionTargetingNodal(&opt, dV_LVLH, P30TIG);

			manopt.dV_LVLH = dV_LVLH;
			manopt.enginetype = SPSRCSDecision(SPS_THRUST / calcParams.src->GetMass(), dV_LVLH);
			manopt.GETbase = GETbase;
			manopt.HeadsUp = false;
			manopt.REFSMMAT = GetREFSMMATfromAGC(&mcc->cm->agc.vagc, AGCEpoch);
			manopt.TIG = P30TIG;
			manopt.vessel = calcParams.src;
			manopt.vesseltype = 1;

			AP11ManeuverPAD(&manopt, *form);
			sprintf(form->purpose, "MCC-4");

			AGCStateVectorUpdate(buffer1, sv, true, AGCEpoch, GETbase);
			AGCExternalDeltaVUpdate(buffer2, P30TIG, dV_LVLH);
			AGCDesiredREFSMMATUpdate(buffer3, REFSMMAT, AGCEpoch);

			sprintf(uplinkdata, "%s%s%s", buffer1, buffer2, buffer3);
			if (upString != NULL) {
				// give to mcc
				strncpy(upString, uplinkdata, 1024 * 3);
				sprintf(upDesc, "CSM state vector, target load, Landing Site REFSMMAT");
			}
		}

	}
	break;
	case 26: //PC+2 UPDATE
	{
		RTEFlybyOpt entopt;
		EntryResults res;
		AP11ManPADOpt opt;
		double GETbase;

		AP11MNV * form = (AP11MNV *)pad;

		GETbase = calcParams.TEPHEM;

		entopt.EntryLng = -165.0*RAD;
		entopt.returnspeed = 2;
		entopt.GETbase = GETbase;
		entopt.FlybyType = 1;
		entopt.vessel = calcParams.src;

		RTEFlybyTargeting(&entopt, &res);

		opt.alt = calcParams.LSAlt;
		opt.dV_LVLH = res.dV_LVLH;
		opt.enginetype = RTCC_ENGINETYPE_SPSDPS;
		opt.GETbase = GETbase;
		opt.HeadsUp = false;
		//LS REFSMMAT
		opt.REFSMMAT = calcParams.StoredREFSMMAT;
		opt.TIG = res.P30TIG;
		opt.vessel = calcParams.src;
		opt.vesseltype = 1;

		AP11ManeuverPAD(&opt, *form);
		sprintf(form->remarks, "Assumes LS REFSMMAT and docked");

		if (!REFSMMATDecision(form->Att*RAD))
		{
			REFSMMATOpt refsopt;
			MATRIX3 REFSMMAT;

			refsopt.dV_LVLH = res.dV_LVLH;
			refsopt.GETbase = GETbase;
			refsopt.P30TIG = res.P30TIG;
			refsopt.REFSMMATopt = 0;
			refsopt.vessel = calcParams.src;

			REFSMMAT = REFSMMATCalc(&refsopt);

			opt.HeadsUp = true;
			opt.REFSMMAT = REFSMMAT;
			AP11ManeuverPAD(&opt, *form);

			sprintf(form->remarks, "Docked, requires realignment to preferred REFSMMAT");
		}
		sprintf(form->purpose, "PC+2");
		form->lat = res.latitude*DEG;
		form->lng = res.longitude*DEG;
		form->RTGO = res.RTGO;
		form->VI0 = res.VIO / 0.3048;
		form->GET05G = res.GET05G;

		//Save parameters for further use
		SplashLatitude = res.latitude;
		SplashLongitude = res.longitude;
		calcParams.TEI = res.P30TIG;
		calcParams.EI = res.GET400K;
	}
	break;
	case 30:	// LOI-1 MANEUVER
	{
		LOIMan opt;
		AP11ManPADOpt manopt;
		double GETbase, P30TIG;
		VECTOR3 dV_LVLH;
		SV sv, sv_n, sv_postLOI;

		AP11MNV * form = (AP11MNV *)pad;

		GETbase = calcParams.TEPHEM;

		sv = StateVectorCalc(calcParams.src); //State vector for uplink

		opt.csmlmdocked = true;
		opt.GETbase = GETbase;
		opt.h_apo = 170.0*1852.0;
		opt.h_peri = 60.0*1852.0;
		opt.alt = calcParams.LSAlt;
		opt.azi = calcParams.LSAzi;
		opt.lat = calcParams.LSLat;
		opt.lng = calcParams.LSLng;
		opt.t_land = calcParams.TLAND;
		opt.vessel = calcParams.src;

		LOITargeting(&opt, dV_LVLH, P30TIG, sv_n, sv_postLOI);

		manopt.alt = calcParams.LSAlt;
		manopt.dV_LVLH = dV_LVLH;
		manopt.enginetype = RTCC_ENGINETYPE_SPSDPS;
		manopt.GETbase = GETbase;
		manopt.HeadsUp = false;
		manopt.REFSMMAT = GetREFSMMATfromAGC(&mcc->cm->agc.vagc, AGCEpoch);
		manopt.TIG = P30TIG;
		manopt.vessel = calcParams.src;
		manopt.vesseltype = 1;

		AP11ManeuverPAD(&manopt, *form);
		sprintf(form->purpose, "LOI-1");
		sprintf(form->remarks, "LM weight is %.0f", form->LMWeight);

		TimeofIgnition = P30TIG;
		DeltaV_LVLH = dV_LVLH;

		char buffer1[1000];
		char buffer2[1000];

		AGCStateVectorUpdate(buffer1, sv, true, AGCEpoch, GETbase);
		AGCExternalDeltaVUpdate(buffer2, P30TIG, dV_LVLH);

		sprintf(uplinkdata, "%s%s", buffer1, buffer2);
		if (upString != NULL) {
			// give to mcc
			strncpy(upString, uplinkdata, 1024 * 3);
			sprintf(upDesc, "CSM state vector, target load");
		}
	}
	break;
	case 31: //LOI-2 MANEUVER
	{
		LOI2Man opt;
		AP11ManPADOpt manopt;
		double GETbase, P30TIG;
		VECTOR3 dV_LVLH;
		SV sv;
		char buffer1[1000];
		char buffer2[1000];

		AP11MNV * form = (AP11MNV *)pad;

		GETbase = calcParams.TEPHEM;

		sv = StateVectorCalc(calcParams.src); //State vector for uplink

		opt.alt = calcParams.LSAlt;
		opt.csmlmdocked = true;
		opt.GETbase = GETbase;
		opt.h_circ = 60.0*1852.0;
		opt.vessel = calcParams.src;

		LOI2Targeting(&opt, dV_LVLH, P30TIG);

		manopt.alt = calcParams.LSAlt;
		manopt.dV_LVLH = dV_LVLH;
		manopt.enginetype = RTCC_ENGINETYPE_SPSDPS;
		manopt.GETbase = GETbase;
		manopt.HeadsUp = false;
		manopt.REFSMMAT = GetREFSMMATfromAGC(&mcc->cm->agc.vagc, AGCEpoch);
		manopt.sxtstardtime = -40.0*60.0;
		manopt.TIG = P30TIG;
		manopt.vessel = calcParams.src;
		manopt.vesseltype = 1;

		AP11ManeuverPAD(&manopt, *form);
		sprintf(form->purpose, "LOI-2");

		TimeofIgnition = P30TIG;
		DeltaV_LVLH = dV_LVLH;

		AGCStateVectorUpdate(buffer1, sv, true, AGCEpoch, GETbase);
		AGCExternalDeltaVUpdate(buffer2, P30TIG, dV_LVLH);

		sprintf(uplinkdata, "%s%s", buffer1, buffer2);
		if (upString != NULL) {
			// give to mcc
			strncpy(upString, uplinkdata, 1024 * 3);
			sprintf(upDesc, "CSM state vector, target load");
		}
	}
	break;
	case 32: //STATE VECTOR and LS REFSMMAT UPLINK
	{
		MATRIX3 REFSMMAT;
		VECTOR3 DV;
		double GETbase, t_DOI, t_PDI, t_land, CR;
		SV sv;
		REFSMMATOpt opt;
		DOIMan doiopt;
		char buffer1[1000];
		char buffer2[1000];

		sv = StateVectorCalc(calcParams.tgt); //State vector for uplink
		GETbase = calcParams.TEPHEM;

		doiopt.alt = calcParams.LSAlt;
		doiopt.EarliestGET = OrbMech::HHMMSSToSS(101, 0, 0);
		doiopt.GETbase = GETbase;
		doiopt.lat = calcParams.LSLat;
		doiopt.lng = calcParams.LSLng;
		doiopt.N = 0;
		doiopt.opt = 0;
		doiopt.sv0 = sv;

		DOITargeting(&doiopt, DV, t_DOI, t_PDI, t_land, CR);
		calcParams.DOI = t_DOI;
		calcParams.PDI = t_PDI;
		calcParams.TLAND = t_land;

		PoweredFlightProcessor(sv, GETbase, t_DOI, RTCC_VESSELTYPE_LM, RTCC_ENGINETYPE_SPSDPS, 0.0, DV, false, TimeofIgnition, DeltaV_LVLH);

		opt.GETbase = GETbase;
		opt.LSLat = calcParams.LSLat;
		opt.LSLng = calcParams.LSLng;
		opt.REFSMMATopt = 5;
		opt.REFSMMATTime = calcParams.TLAND;
		opt.vessel = calcParams.src;

		REFSMMAT = REFSMMATCalc(&opt);
		calcParams.StoredREFSMMAT = REFSMMAT;

		AGCStateVectorUpdate(buffer1, sv, true, AGCEpoch, GETbase);
		AGCDesiredREFSMMATUpdate(buffer2, REFSMMAT, AGCEpoch);

		sprintf(uplinkdata, "%s%s", buffer1, buffer2);
		if (upString != NULL) {
			// give to mcc
			strncpy(upString, uplinkdata, 1024 * 3);
			sprintf(upDesc, "CSM state vector, LS REFSMMAT");
		}
	}
	break;
	case 33: //CSM DAP DATA
	{
		AP10DAPDATA * form = (AP10DAPDATA *)pad;

		CSMDAPUpdate(calcParams.src, *form);
	}
	break;
	case 34: //LM DAP DATA
	{
		AP10DAPDATA * form = (AP10DAPDATA *)pad;

		LMDAPUpdate(calcParams.tgt, *form);
	}
	break;
	case 35: //GYRO TORQUING ANGLES
	{
		TORQANG * form = (TORQANG *)pad;
		LEM *lem = (LEM *)calcParams.tgt;

		VECTOR3 lmn20, csmn20, V42angles;

		csmn20.x = calcParams.src->imu.Gimbal.X;
		csmn20.y = calcParams.src->imu.Gimbal.Y;
		csmn20.z = calcParams.src->imu.Gimbal.Z;

		lmn20.x = lem->imu.Gimbal.X;
		lmn20.y = lem->imu.Gimbal.Y;
		lmn20.z = lem->imu.Gimbal.Z;

		V42angles = OrbMech::LMDockedFineAlignment(lmn20, csmn20);

		form->V42Angles.x = V42angles.x*DEG;
		form->V42Angles.y = V42angles.y*DEG;
		form->V42Angles.z = V42angles.z*DEG;
	}
	break;
	case 36: //LGC ACTIVATION UPDATE
	{
		AP11AGSACT *form = (AP11AGSACT*)pad;

		SV sv;
		MATRIX3 REFSMMAT;
		double GETbase;
		char buffer1[1000];
		char buffer2[1000];
		char buffer3[1000];

		sv = StateVectorCalc(calcParams.src); //State vector for uplink
		GETbase = calcParams.TEPHEM;

		REFSMMAT = calcParams.StoredREFSMMAT;

		form->KFactor = 90.0*3600.0;
		form->DEDA224 = 60267;
		form->DEDA225 = 58148;
		form->DEDA226 = 70312;
		form->DEDA227 = -50031;

		AGCStateVectorUpdate(buffer1, sv, false, AGCEpoch, GETbase);
		AGCStateVectorUpdate(buffer2, sv, true, AGCEpoch, GETbase);
		AGCREFSMMATUpdate(buffer3, REFSMMAT, AGCEpoch, LGCREFSAddrOffs);

		sprintf(uplinkdata, "%s%s%s", buffer1, buffer2, buffer3);
		if (upString != NULL) {
			// give to mcc
			strncpy(upString, uplinkdata, 1024 * 3);
			sprintf(upDesc, "State vectors, LS REFSMMAT");
		}
	}
	break;
	case 37: //SEPARATION MANEUVER
	{
		AP11ManPADOpt opt;
		SV sv;
		VECTOR3 dV_LVLH;
		double GETbase, t_P, mu, t_Sep;
		char buffer1[1000];
		char buffer2[1000];

		AP11MNV * form = (AP11MNV *)pad;

		sv = StateVectorCalc(calcParams.src); //State vector for uplink
		GETbase = calcParams.TEPHEM;
		mu = GGRAV * oapiGetMass(sv.gravref);

		t_P = OrbMech::period(sv.R, sv.V, mu);
		t_Sep = floor(calcParams.DOI - t_P / 2.0);
		calcParams.SEP = t_Sep;
		dV_LVLH = _V(0, 0, -2.5)*0.3048;

		opt.alt = calcParams.LSAlt;
		opt.dV_LVLH = dV_LVLH;
		opt.enginetype = RTCC_ENGINETYPE_RCS;
		opt.GETbase = GETbase;
		opt.HeadsUp = true;
		opt.REFSMMAT = GetREFSMMATfromAGC(&mcc->cm->agc.vagc, AGCEpoch);
		opt.TIG = t_Sep;
		opt.vessel = calcParams.src;
		opt.vesseltype = 0;

		AP11ManeuverPAD(&opt, *form);

		AGCStateVectorUpdate(buffer1, sv, true, AGCEpoch, GETbase);
		AGCStateVectorUpdate(buffer2, sv, false, AGCEpoch, GETbase);

		sprintf(uplinkdata, "%s%s", buffer1, buffer2);
		if (upString != NULL) {
			// give to mcc
			strncpy(upString, uplinkdata, 1024 * 3);
			sprintf(upDesc, "State vectors");
		}
	}
	break;
	case 38: //DESCENT ORBIT INSERTION
	{
		AP11LMManPADOpt opt;

		VECTOR3 DV;
		double GETbase, t_PDI, t_land, CR, t_DOI_imp;
		SV sv, sv_uplink;
		DOIMan doiopt;
		char TLANDbuffer[64];
		char buffer1[1000];
		char buffer2[1000];

		AP11LMMNV * form = (AP11LMMNV *)pad;

		sv = StateVectorCalc(calcParams.tgt);
		GETbase = calcParams.TEPHEM;

		doiopt.alt = calcParams.LSAlt;
		doiopt.EarliestGET = OrbMech::HHMMSSToSS(101, 0, 0);
		doiopt.GETbase = GETbase;
		doiopt.lat = calcParams.LSLat;
		doiopt.lng = calcParams.LSLng;
		doiopt.N = 0;
		doiopt.opt = 0;
		doiopt.sv0 = sv;

		DOITargeting(&doiopt, DV, t_DOI_imp, t_PDI, t_land, CR);

		calcParams.DOI = t_DOI_imp;
		calcParams.PDI = t_PDI;
		calcParams.TLAND = t_land;

		PoweredFlightProcessor(sv, GETbase, t_DOI_imp, RTCC_VESSELTYPE_LM, RTCC_ENGINETYPE_SPSDPS, 0.0, DV, false, TimeofIgnition, DeltaV_LVLH);

		opt.alt = calcParams.LSAlt;
		opt.csmlmdocked = false;
		opt.dV_LVLH = DeltaV_LVLH;
		opt.enginetype = RTCC_ENGINETYPE_SPSDPS;
		opt.GETbase = GETbase;
		opt.HeadsUp = true;
		opt.REFSMMAT = GetREFSMMATfromAGC(&mcc->lm->agc.vagc, AGCEpoch, LGCREFSAddrOffs);
		opt.TIG = TimeofIgnition;
		opt.vessel = calcParams.tgt;

		AP11LMManeuverPAD(&opt, *form);
		sprintf(form->purpose, "DOI");

		//Time tagged to DOI-10
		sv_uplink = StateVectorCalc(calcParams.tgt, OrbMech::MJDfromGET(calcParams.DOI - 10.0*60.0, GETbase));

		AGCStateVectorUpdate(buffer1, sv_uplink, false, AGCEpoch, GETbase);
		AGCExternalDeltaVUpdate(buffer2, TimeofIgnition, DeltaV_LVLH, LGCDeltaVAddr);
		TLANDUpdate(TLANDbuffer, calcParams.TLAND, 2400);

		sprintf(uplinkdata, "%s%s%s", buffer1, buffer2, TLANDbuffer);
		if (upString != NULL) {
			// give to mcc
			strncpy(upString, uplinkdata, 1024 * 3);
			sprintf(upDesc, "LM state vector, DOI target load");
		}
	}
	break;
	case 40: //TEI-1 UPDATE (PRE LOI-1)
	case 41: //TEI-4 UPDATE (PRE LOI-1)
	case 42: //TEI-5 UPDATE (PRE LOI-2)
	case 43: //TEI-11 UPDATE
	case 44: //TEI-30 UPDATE
	{
		TEIOpt entopt;
		EntryResults res;
		AP11ManPADOpt opt;
		double GETbase;
		SV sv0, sv1, sv2;
		char manname[8];

		AP11MNV * form = (AP11MNV *)pad;

		GETbase = calcParams.TEPHEM;
		sv0 = StateVectorCalc(calcParams.src); //State vector for uplink

		//Simulate the maneuver preceeding TEI (LOI-1 or LOI-2)
		if (fcn == 40 || fcn == 41 || fcn == 42)
		{
			sv1 = ExecuteManeuver(calcParams.src, GETbase, TimeofIgnition, DeltaV_LVLH, sv0, GetDockedVesselMass(calcParams.src));
		}
		else
		{
			sv1 = sv0;
		}

		if (fcn == 40)
		{
			sprintf(manname, "TEI-1");
			sv2 = coast(sv1, 0.5*2.0*3600.0);
		}
		else if (fcn == 41)
		{
			sprintf(manname, "TEI-4");
			sv2 = coast(sv1, 3.5*2.0*3600.0);
		}
		else if (fcn == 42)
		{
			sprintf(manname, "TEI-5");
			sv2 = coast(sv1, 2.5*2.0*3600.0);
		}
		else if (fcn == 43)
		{
			sprintf(manname, "TEI-11");
			sv2 = coast(sv1, 6.5*2.0*3600.0);
		}
		else if (fcn == 44)
		{
			sprintf(manname, "TEI-30");
			sv2 = coast(sv1, 19.5*2.0*3600.0);
		}

		entopt.EntryLng = -165.0*RAD;
		entopt.GETbase = GETbase;
		entopt.Inclination = 40.0*RAD;
		entopt.Ascending = true;
		entopt.returnspeed = 1;
		entopt.RV_MCC = sv2;
		entopt.useSV = true;
		entopt.vessel = calcParams.src;

		TEITargeting(&entopt, &res);

		opt.alt = calcParams.LSAlt;
		opt.dV_LVLH = res.dV_LVLH;
		opt.enginetype = RTCC_ENGINETYPE_SPSDPS;
		opt.GETbase = GETbase;
		opt.HeadsUp = false;
		opt.REFSMMAT = GetREFSMMATfromAGC(&mcc->cm->agc.vagc, AGCEpoch);
		opt.RV_MCC = sv1;
		opt.TIG = res.P30TIG;
		opt.useSV = true;
		opt.vessel = calcParams.src;
		opt.vesseltype = 0;

		AP11ManeuverPAD(&opt, *form);
		sprintf(form->purpose, manname);
		form->lat = res.latitude*DEG;
		form->lng = res.longitude*DEG;
		form->RTGO = res.RTGO;
		form->VI0 = res.VIO / 0.3048;
		form->GET05G = res.GET05G;

		if (fcn == 40)
		{
			sprintf(form->remarks, "Undocked");
		}
		else if (fcn == 41)
		{
			sprintf(form->remarks, "Undocked, assumes no LOI-2");
		}
		else if (fcn == 42)
		{
			sprintf(form->remarks, "Undocked, assumes LOI-2");
		}

		//Save parameters for further use
		SplashLatitude = res.latitude;
		SplashLongitude = res.longitude;
		calcParams.TEI = res.P30TIG;
		calcParams.EI = res.GET400K;
	}
	break;
	case 60: //REV 1 MAP UPDATE
	{
		SV sv0, sv1, sv2;
		AP10MAPUPDATE upd_hyper, upd_ellip;
		double GETbase;

		AP10MAPUPDATE * form = (AP10MAPUPDATE *)pad;

		GETbase = calcParams.TEPHEM;
		sv0 = StateVectorCalc(calcParams.src);
		LunarOrbitMapUpdate(sv0, GETbase, upd_hyper);

		sv1 = ExecuteManeuver(calcParams.src, GETbase, TimeofIgnition, DeltaV_LVLH, sv0, GetDockedVesselMass(calcParams.src));
		sv2 = coast(sv1, -30.0*60.0);
		LunarOrbitMapUpdate(sv2, GETbase, upd_ellip);

		form->Rev = 1;
		form->type = 2;
		form->AOSGET = upd_hyper.AOSGET;
		form->LOSGET = upd_hyper.LOSGET;
		form->PMGET = upd_ellip.AOSGET;
	}
	break;
	case 61: //REV 4 LANDMARK TRACKING PAD A-1
	case 62: //REV 12 LANDMARK TRACKING PAD LMK 130
	case 63: //LM TRACKING PAD
	{
		LMARKTRKPADOpt opt;

		AP11LMARKTRKPAD * form = (AP11LMARKTRKPAD *)pad;

		opt.GETbase = calcParams.TEPHEM;
		opt.vessel = calcParams.src;

		if (fcn == 61)
		{
			sprintf(form->LmkID[0], "A-1");
			opt.alt[0] = 0;
			opt.lat[0] = 2.0*RAD;
			opt.LmkTime[0] = OrbMech::HHMMSSToSS(82, 40, 0);
			opt.lng[0] = 65.5*RAD;
			opt.entries = 1;
		}
		else if (fcn == 62)
		{
			sprintf(form->LmkID[0], "130");
			opt.alt[0] = -1.46*1852.0;
			opt.lat[0] = 1.243*RAD;
			opt.LmkTime[0] = OrbMech::HHMMSSToSS(98, 30, 0);
			opt.lng[0] = 23.688*RAD;
			opt.entries = 1;
		}
		else if (fcn == 63)
		{
			double LSRad, R_M;
			R_M = 1.73809e6;

			//Update landing site
			calcParams.tgt->GetEquPos(calcParams.LSLng, calcParams.LSLat, LSRad);
			calcParams.LSAlt = LSRad - R_M;

			sprintf(form->LmkID[0], "Lunar Module");
			opt.alt[0] = calcParams.LSAlt;
			opt.lat[0] = calcParams.LSLat;
			opt.LmkTime[0] = OrbMech::HHMMSSToSS(104, 30, 0);
			opt.lng[0] = calcParams.LSLng;
			opt.entries = 1;
		}

		LandmarkTrackingPAD(&opt, *form);
	}
	break;
	case 70: //PDI PAD
	{
		AP11PDIPAD * form = (AP11PDIPAD *)pad;

		PDIPADOpt opt;
		double GETbase, rad;

		GETbase = calcParams.TEPHEM;
		rad = oapiGetSize(oapiGetObjectByName("Moon"));

		opt.direct = false;
		opt.dV_LVLH = DeltaV_LVLH;
		opt.GETbase = GETbase;
		opt.HeadsUp = false;
		opt.P30TIG = TimeofIgnition;
		opt.REFSMMAT = GetREFSMMATfromAGC(&mcc->lm->agc.vagc, AGCEpoch, LGCREFSAddrOffs);
		opt.R_LS = OrbMech::r_from_latlong(calcParams.LSLat, calcParams.LSLng, calcParams.LSAlt + rad);
		opt.t_land = calcParams.TLAND;
		opt.vessel = calcParams.tgt;

		PDI_PAD(&opt, *form);
	}
	break;
	case 71: //PDI ABORT PAD
	case 75: //CSM RESCUE PAD
	{
		PDIABORTPAD * form = (PDIABORTPAD *)pad;

		SV sv;
		double GETbase, t_sunrise1, t_sunrise2, t_TPI;

		sv = StateVectorCalc(calcParams.src);
		GETbase = calcParams.TEPHEM;

		t_sunrise1 = calcParams.PDI + 3.0*3600.0;
		t_sunrise2 = calcParams.PDI + 5.0*3600.0;

		//Find two TPI opportunities
		t_TPI = FindOrbitalSunrise(sv, GETbase, t_sunrise1) - 23.0*60.0;
		form->T_TPI_Pre10Min = round(t_TPI);
		t_TPI = FindOrbitalSunrise(sv, GETbase, t_sunrise2) - 23.0*60.0;
		form->T_TPI_Post10Min = round(t_TPI);

		//Phasing 67 minutes after PDI
		form->T_Phasing = round(calcParams.PDI + 67.0*60.0);

		if (fcn == 75)
		{
			form->type = 1;
		}
	}
	break;
	case 72: //No PDI+12 PAD
	{
		AP11LMMNV * form = (AP11LMMNV*)pad;

		LambertMan opt;
		AP11LMManPADOpt manopt;
		TwoImpulseResuls res;
		SV sv_LM, sv_DOI, sv_CSM;
		VECTOR3 dV_LVLH;
		double GETbase, t_sunrise, t_CSI, t_TPI, dt, P30TIG, t_P, mu;

		GETbase = calcParams.TEPHEM;
		sv_CSM = StateVectorCalc(calcParams.src);
		sv_LM = StateVectorCalc(calcParams.tgt);

		sv_DOI = ExecuteManeuver(calcParams.tgt, GETbase, TimeofIgnition, DeltaV_LVLH, sv_LM, 0.0);

		t_sunrise = calcParams.PDI + 3.0*3600.0;
		t_TPI = FindOrbitalSunrise(sv_CSM, GETbase, t_sunrise) - 23.0*60.0;

		opt.axis = RTCC_LAMBERT_MULTIAXIS;
		opt.DH = 15.0*1852.0;
		opt.Elevation = 26.6*RAD;
		opt.GETbase = GETbase;
		opt.N = 0;
		opt.NCC_NSR_Flag = true;
		opt.Perturbation = RTCC_LAMBERT_PERTURBED;
		opt.PhaseAngle = -4.47*RAD;
		opt.sv_A = sv_DOI;
		opt.sv_P = sv_CSM;
		//PDI+12
		opt.T1 = calcParams.PDI + 12.0*60.0;
		//Estimate for T2
		opt.T2 = opt.T1 + 3600.0 + 46.0*60.0;
		opt.use_XYZ_Offset = false;

		for (int i = 0;i < 2;i++)
		{
			LambertTargeting(&opt, res);
			dt = t_TPI - res.t_TPI;
			opt.T2 += dt;
		}

		mu = GGRAV * oapiGetMass(sv_DOI.gravref);
		t_P = OrbMech::period(sv_DOI.R, sv_DOI.V + res.dV, mu);
		t_CSI = opt.T2 - t_P / 2.0;

		PoweredFlightProcessor(sv_DOI, GETbase, opt.T1, RTCC_VESSELTYPE_LM, RTCC_ENGINETYPE_SPSDPS, 0.0, res.dV, false, P30TIG, dV_LVLH);
		//Store for P76 PAD
		calcParams.TIGSTORE1 = P30TIG;
		calcParams.DVSTORE1 = dV_LVLH;

		manopt.alt = calcParams.LSAlt;
		manopt.dV_LVLH = dV_LVLH;
		manopt.enginetype = RTCC_ENGINETYPE_SPSDPS;
		manopt.GETbase = GETbase;
		manopt.HeadsUp = false;
		manopt.REFSMMAT = GetREFSMMATfromAGC(&mcc->lm->agc.vagc, AGCEpoch, LGCREFSAddrOffs);
		manopt.RV_MCC = sv_DOI;
		manopt.TIG = P30TIG;
		manopt.useSV = true;
		manopt.vessel = calcParams.tgt;

		AP11LMManeuverPAD(&manopt, *form);
		sprintf(form->purpose, "No PDI +12 ABORT");

		form->type = 1;
		form->t_CSI = round(t_CSI);
		form->t_TPI = round(t_TPI);
	}
	break;
	case 73: //T2 ABORT PAD
	{
		AP11T2ABORTPAD * form = (AP11T2ABORTPAD*)pad;

		SV sv_CSM, sv_Ins, sv_CSM_upl;
		VECTOR3 R_LS, R_C1, V_C1, u, V_C1F, R_CSI1, V_CSI1;
		double T2, rad, GETbase, m0, v_LH, v_LV, theta, dt_asc, t_C1, mu, dt1, dt2, t_CSI1, t_sunrise, t_TPI;
		char buffer1[1000];

		GETbase = calcParams.TEPHEM;
		sv_CSM = StateVectorCalc(calcParams.src);

		rad = oapiGetSize(sv_CSM.gravref);
		mu = GGRAV * oapiGetMass(sv_CSM.gravref);
		
		LEM *l = (LEM*)calcParams.tgt;
		m0 = l->GetAscentStageMass();

		v_LH = 5515.2*0.3048;
		v_LV = 19.6*0.3048;

		T2 = calcParams.PDI + 21.0*60.0 + 24.0;
		R_LS = OrbMech::r_from_latlong(calcParams.LSLat, calcParams.LSLng, calcParams.LSAlt + rad);

		LunarAscentProcessor(R_LS, m0, sv_CSM, GETbase, T2, v_LH, v_LV, theta, dt_asc, sv_Ins);
		dt1 = OrbMech::timetoapo(sv_Ins.R, sv_Ins.V, mu);
		OrbMech::rv_from_r0v0(sv_Ins.R, sv_Ins.V, dt1, R_C1, V_C1, mu);
		t_C1 = T2 + dt_asc + dt1;
		u = unit(crossp(R_C1, V_C1));
		V_C1F = V_C1 + unit(crossp(u, V_C1))*10.0*0.3048;
		OrbMech::REVUP(R_C1, V_C1F, 1.5, mu, R_CSI1, V_CSI1, dt2);
		t_CSI1 = t_C1 + dt2;

		t_sunrise = calcParams.PDI + 7.0*3600.0;
		t_TPI = FindOrbitalSunrise(sv_CSM, GETbase, t_sunrise) - 23.0*60.0;

		form->TIG = T2;
		form->t_CSI1 = round(t_CSI1);
		form->t_Phasing = round(t_C1);
		form->t_TPI = round(t_TPI);

		//CSM state vector, time tagged PDI+25 minutes
		sv_CSM_upl = StateVectorCalc(calcParams.src, OrbMech::MJDfromGET(calcParams.PDI + 25.0*60.0, GETbase));
		AGCStateVectorUpdate(buffer1, sv_CSM_upl, true, AGCEpoch, GETbase);

		sprintf(uplinkdata, "%s", buffer1);
		if (upString != NULL) {
			// give to mcc
			strncpy(upString, uplinkdata, 1024 * 3);
			sprintf(upDesc, "CSM state vector");
		}
	}
	break;
	case 74: //T3 ABORT PAD
	{
		AP11T3ABORTPAD * form = (AP11T3ABORTPAD*)pad;

		LunarLiftoffTimeOpt opt;
		LunarLiftoffResults res;
		SV sv_CSM, sv_CSM2, sv_CSM_over, sv_Ins;
		VECTOR3 R_LS;
		double GETbase, m0, MJD_over, t_P, mu, t_PPlusDT, theta_1, dt_1, R_M;

		GETbase = calcParams.TEPHEM;
		sv_CSM = StateVectorCalc(calcParams.src);
		mu = GGRAV * oapiGetMass(sv_CSM.gravref);

		LEM *l = (LEM*)calcParams.tgt;
		m0 = l->GetAscentStageMass();

		opt.alt = calcParams.LSAlt;
		opt.GETbase = GETbase;
		opt.lat = calcParams.LSLat;
		opt.lng = calcParams.LSLng;
		opt.sv_CSM = sv_CSM;
		opt.t_hole = calcParams.PDI + 1.5*3600.0;

		R_M = oapiGetSize(sv_CSM.gravref);
		R_LS = OrbMech::r_from_latlong(calcParams.LSLat, calcParams.LSLng, R_M + calcParams.LSAlt);

		//Initial pass through the processor
		LaunchTimePredictionProcessor(&opt, &res);
		//Refine ascent parameters
		LunarAscentProcessor(R_LS, m0, sv_CSM, GETbase, res.t_L, res.v_LH, res.v_LV, theta_1, dt_1, sv_Ins);
		opt.theta_1 = theta_1;
		opt.dt_1 = dt_1;
		//Final pass through
		LaunchTimePredictionProcessor(&opt, &res);

		sv_CSM2 = GeneralTrajectoryPropagation(sv_CSM, 0, OrbMech::MJDfromGET(calcParams.PDI, GETbase));
		MJD_over = OrbMech::P29TimeOfLongitude(sv_CSM2.R, sv_CSM2.V, sv_CSM2.MJD, sv_CSM2.gravref, calcParams.LSLng);
		sv_CSM_over = coast(sv_CSM2, (MJD_over - sv_CSM2.MJD)*24.0*3600.0);

		t_P = OrbMech::period(sv_CSM_over.R, sv_CSM_over.V, mu);
		t_PPlusDT = res.t_L - OrbMech::GETfromMJD(sv_CSM_over.MJD, GETbase);

		form->TIG = round(res.t_L);
		form->t_CSI = round(res.t_CSI);
		form->t_Period = t_P;
		form->t_PPlusDT = t_PPlusDT;
		form->t_TPI = round(res.t_TPI);
	}
	break;
	case 76: //P76 PAD for DOI AND NO PDI+12
	{
		AP11P76PAD * form = (AP11P76PAD*)pad;

		SV sv_CSM, sv_LM;
		double GETbase;
		char buffer1[1000];
		char buffer2[1000];

		GETbase = calcParams.TEPHEM;

		//State vectors: CSM SV time tagged PDI+25, LM SV time tagged DOI-10
		sv_CSM = StateVectorCalc(calcParams.src, OrbMech::MJDfromGET(calcParams.PDI + 25.0*60.0, GETbase));
		sv_LM = StateVectorCalc(calcParams.tgt, OrbMech::MJDfromGET(calcParams.DOI - 10.0*60.0, GETbase));

		form->entries = 2;
		sprintf(form->purpose[0], "DOI");
		form->TIG[0] = TimeofIgnition;
		form->DV[0] = DeltaV_LVLH / 0.3048;
		sprintf(form->purpose[1], "PDI1 +12");
		form->TIG[1] = calcParams.TIGSTORE1;
		form->DV[1] = calcParams.DVSTORE1 / 0.3048;

		AGCStateVectorUpdate(buffer1, sv_CSM, true, AGCEpoch, GETbase);
		AGCStateVectorUpdate(buffer2, sv_LM, false, AGCEpoch, GETbase);

		sprintf(uplinkdata, "%s%s", buffer1, buffer2);
		if (upString != NULL) {
			// give to mcc
			strncpy(upString, uplinkdata, 1024 * 3);
			sprintf(upDesc, "CSM and LM state vectors");
		}
	}
	break;
	case 77: //DOI EVALUATION
	{
		sprintf(upMessage, "Go for PDI");
	}
	break;
	case 78: //PDI EVALUATION
	{

	}
	break;
	case 79: //LANDING EVALUATION
	{
		sprintf(upMessage, "We copy you down, Eagle");
	}
	break;
	case 80: //STAY FOR T1
	{
		sprintf(upMessage, "Stay for T1");
	}
	break;
	case 81: //STAY FOR T2
	{
		sprintf(upMessage, "Stay for T2");
	}
	break;
	case 90: //LM ASCENT PAD FOR T3
	{
		AP11LMASCPAD * form = (AP11LMASCPAD*)pad;

		LunarLiftoffTimeOpt opt;
		LunarLiftoffResults res;
		SV sv_CSM, sv_CSM_upl, sv_Ins;
		VECTOR3 R_LS;
		char buffer1[100], buffer2[1000];
		double GETbase, m0, theta_1, dt_1, R_M;

		GETbase = calcParams.TEPHEM;
		sv_CSM = StateVectorCalc(calcParams.src);

		LEM *l = (LEM*)calcParams.tgt;
		m0 = l->GetAscentStageMass();

		opt.alt = calcParams.LSAlt;
		opt.GETbase = GETbase;
		opt.lat = calcParams.LSLat;
		opt.lng = calcParams.LSLng;
		opt.sv_CSM = sv_CSM;
		opt.t_hole = calcParams.PDI + 1.5*3600.0;

		R_M = oapiGetSize(sv_CSM.gravref);
		R_LS = OrbMech::r_from_latlong(calcParams.LSLat, calcParams.LSLng, R_M + calcParams.LSAlt);

		//Initial pass through the processor
		LaunchTimePredictionProcessor(&opt, &res);
		//Refine ascent parameters
		LunarAscentProcessor(R_LS, m0, sv_CSM, GETbase, res.t_L, res.v_LH, res.v_LV, theta_1, dt_1, sv_Ins);
		opt.theta_1 = theta_1;
		opt.dt_1 = dt_1;
		//Final pass through
		LaunchTimePredictionProcessor(&opt, &res);

		calcParams.LunarLiftoff = res.t_L;
		calcParams.CSI = res.t_CSI;
		calcParams.CDH = res.t_CDH;
		calcParams.TPI = res.t_TPI;

		form->TIG = res.t_L;
		form->V_hor = res.v_LH / 0.3048;
		form->V_vert = res.v_LV / 0.3048;
		form->DEDA225_226 = GetSemiMajorAxis(sv_Ins) / 0.3048 / 100.0;
		form->DEDA231 = length(R_LS) / 0.3048 / 100.0;

		//Store for CSI PAD
		calcParams.DVSTORE1 = _V(res.DV_CSI, 0, 0);
		calcParams.SVSTORE1 = sv_Ins;

		LandingSiteUplink(buffer1, calcParams.LSLat, calcParams.LSLng, calcParams.LSAlt, 2022);
		sv_CSM_upl = StateVectorCalc(calcParams.src, OrbMech::MJDfromGET(calcParams.TLAND + 100.0*60.0, GETbase));
		AGCStateVectorUpdate(buffer2, sv_CSM_upl, true, AGCEpoch, GETbase);

		sprintf(uplinkdata, "%s%s", buffer1, buffer2);
		if (upString != NULL) {
			// give to mcc
			strncpy(upString, uplinkdata, 1024 * 3);
			sprintf(upDesc, "RLS, CSM state vector");
		}
	}
	break;
	case 91: //CSI PAD FOR T3
	{
		AP10CSI * form = (AP10CSI*)pad;

		VECTOR3 dV_AGS;
		double KFactor, GETbase, TIG;

		GETbase = calcParams.TEPHEM;
		KFactor = 90.0*3600.0;

		PoweredFlightProcessor(calcParams.SVSTORE1, GETbase, calcParams.CSI, RTCC_VESSELTYPE_LM, RTCC_ENGINETYPE_RCS, 0.0, calcParams.DVSTORE1, true, TIG, dV_AGS, false);

		form->DEDA373 = (calcParams.CSI - KFactor) / 60.0;
		form->DEDA275 = (calcParams.TPI - KFactor) / 60.0;
		form->dV_AGS = dV_AGS / 0.3048;
		form->dV_LVLH = calcParams.DVSTORE1 / 0.3048;
		form->PLM_FDAI = 0.0;
		form->t_CSI = round(calcParams.CSI);
		form->t_TPI = round(calcParams.TPI);
		form->type = 1;
	}
	break;
	case 92: //LIFTOFF TIME UPDATE T4 to T7
	case 97: //LIFTOFF TIME UPDATE T8 to T10
	{
		LIFTOFFTIMES * form = (LIFTOFFTIMES*)pad;

		LunarLiftoffTimeOpt opt;
		LunarLiftoffResults res;
		SV sv_CSM, sv_CSM_upl, sv_Ins;
		VECTOR3 R_LS;
		double GETbase, m0, theta_1, dt_1, R_M;

		GETbase = calcParams.TEPHEM;
		sv_CSM = StateVectorCalc(calcParams.src);

		LEM *l = (LEM*)calcParams.tgt;
		m0 = l->GetAscentStageMass();

		opt.alt = calcParams.LSAlt;
		opt.GETbase = GETbase;
		opt.lat = calcParams.LSLat;
		opt.lng = calcParams.LSLng;
		opt.sv_CSM = sv_CSM;
		if (fcn == 92)
		{
			opt.t_hole = calcParams.PDI + 3.5*3600.0;
			form->entries = 4;
			form->startdigit = 4;
		}
		else if (fcn == 97)
		{
			opt.t_hole = calcParams.PDI + 11.5*3600.0;
			form->entries = 3;
			form->startdigit = 8;
		}

		R_M = oapiGetSize(sv_CSM.gravref);
		R_LS = OrbMech::r_from_latlong(calcParams.LSLat, calcParams.LSLng, R_M + calcParams.LSAlt);

		//Initial pass through the processor
		LaunchTimePredictionProcessor(&opt, &res);
		//Refine ascent parameters
		LunarAscentProcessor(R_LS, m0, sv_CSM, GETbase, res.t_L, res.v_LH, res.v_LV, theta_1, dt_1, sv_Ins);
		opt.theta_1 = theta_1;
		opt.dt_1 = dt_1;


		for (int i = 0;i < form->entries;i++)
		{
			LaunchTimePredictionProcessor(&opt, &res);
			form->TIG[i] = res.t_L;
			opt.t_hole += 2.0*3600.0;
		}
	}
	break;
	case 93: //PLANE CHANGE EVALUATION
	{
		SV sv_CSM, sv_Liftoff;
		VECTOR3 R_LS;
		double TIG_nom, GETbase, MJD_TIG_nom, dt1, LmkRange;

		sv_CSM = StateVectorCalc(calcParams.src);
		GETbase = calcParams.TEPHEM;

		//Initial guess for liftoff time
		calcParams.LunarLiftoff = OrbMech::HHMMSSToSS(124, 23, 26);
		TIG_nom = calcParams.LunarLiftoff;
		MJD_TIG_nom = OrbMech::MJDfromGET(TIG_nom, GETbase);
		sv_Liftoff = GeneralTrajectoryPropagation(sv_CSM, 0, MJD_TIG_nom);

		R_LS = RLS_from_latlng(calcParams.LSLat, calcParams.LSLng, calcParams.LSAlt);

		dt1 = OrbMech::findelev_gs(sv_Liftoff.R, sv_Liftoff.V, R_LS, MJD_TIG_nom, 180.0*RAD, sv_Liftoff.gravref, LmkRange);

		if (abs(LmkRange) < 4.0*1852.0)
		{
			sprintf(upMessage, "Plane Change has been scrubbed");
			scrubbed = true;
		}
	}
	break;
	case 94: //PLANE CHANGE TARGETING (FOR REFSMMAT)
	case 95: //PLANE CHANGE TARGETING (FOR BURN)
	{
		PCMan opt;
		double GETbase;

		GETbase = calcParams.TEPHEM;

		opt.alt = calcParams.LSAlt;
		opt.EarliestGET = OrbMech::HHMMSSToSS(106, 30, 0);
		opt.GETbase = GETbase;
		opt.lat = calcParams.LSLat;
		opt.lng = calcParams.LSLng;
		opt.t_A = calcParams.LunarLiftoff;
		opt.vessel = calcParams.src;
		opt.vesseltype = 0;

		PlaneChangeTargeting(&opt, DeltaV_LVLH, TimeofIgnition);

		if (fcn == 94)
		{
			REFSMMATOpt refsopt;
			MATRIX3 REFSMMAT;
			char buffer[1000];

			refsopt.dV_LVLH = DeltaV_LVLH;
			refsopt.GETbase = GETbase;
			refsopt.HeadsUp = false;
			refsopt.P30TIG = TimeofIgnition;
			refsopt.REFSMMATopt = 0;
			refsopt.vessel = calcParams.src;
			refsopt.vesseltype = 0;

			REFSMMAT = REFSMMATCalc(&refsopt);
			AGCDesiredREFSMMATUpdate(buffer, REFSMMAT, AGCEpoch);

			sprintf(uplinkdata, "%s", buffer);
			if (upString != NULL) {
				// give to mcc
				strncpy(upString, uplinkdata, 1024 * 3);
				sprintf(upDesc, "PC REFSMMAT");
			}
		}
		else
		{
			AP11MNV * form = (AP11MNV*)pad;

			AP11ManPADOpt manopt;
			SV sv_CSM;
			char buffer1[1000], buffer2[1000];

			sv_CSM = StateVectorCalc(calcParams.src);

			manopt.alt = calcParams.LSAlt;
			manopt.dV_LVLH = DeltaV_LVLH;
			manopt.enginetype = RTCC_ENGINETYPE_SPSDPS;
			manopt.GETbase = GETbase;
			manopt.HeadsUp = false;
			manopt.REFSMMAT = GetREFSMMATfromAGC(&mcc->cm->agc.vagc, AGCEpoch);
			manopt.TIG = TimeofIgnition;
			manopt.vessel = calcParams.src;
			manopt.vesseltype = 0;

			AP11ManeuverPAD(&manopt, *form);
			sprintf(form->purpose, "PLANE CHANGE");

			AGCStateVectorUpdate(buffer1, sv_CSM, true, AGCEpoch, GETbase);
			AGCExternalDeltaVUpdate(buffer2, TimeofIgnition, DeltaV_LVLH);

			sprintf(uplinkdata, "%s%s", buffer1, buffer2);
			if (upString != NULL) {
				// give to mcc
				strncpy(upString, uplinkdata, 1024 * 3);
				sprintf(upDesc, "CSM state vector, target load");
			}
		}
	}
	break;
	case 96: //CMC LUNAR LIFTOFF REFSMMAT UPLINK
	{
		REFSMMATOpt opt;
		MATRIX3 REFSMMAT;
		double GETbase;
		char buffer[1000];

		GETbase = calcParams.TEPHEM;

		opt.GETbase = GETbase;
		opt.LSLat = calcParams.LSLat;
		opt.LSLng = calcParams.LSLng;
		opt.REFSMMATopt = 5;
		opt.REFSMMATTime = calcParams.LunarLiftoff;
		opt.vessel = calcParams.src;

		REFSMMAT = REFSMMATCalc(&opt);
		AGCDesiredREFSMMATUpdate(buffer, REFSMMAT, AGCEpoch);

		sprintf(uplinkdata, "%s", buffer);
		if (upString != NULL) {
			// give to mcc
			strncpy(upString, uplinkdata, 1024 * 3);
			sprintf(upDesc, "Liftoff REFSMMAT");
		}
	}
	break;
	}

	return scrubbed;
}
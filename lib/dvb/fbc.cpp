#include <lib/dvb/fbc.h>
#include <lib/dvb/dvb.h>
#include <lib/dvb/sec.h>
#include <lib/base/object.h>

#include <unistd.h>
#include <fcntl.h>

DEFINE_REF(eFBCTunerManager);

bool eFBCTunerManager::isDestroyed = false;

eFBCTunerManager::eFBCTunerManager()
{
	ePtr<eDVBResourceManager> res_mgr;
	eDVBResourceManager::getInstance(res_mgr);
	m_res_mgr = res_mgr;

	/* number of fbc tuners in one set */
	m_fbc_tuner_num = getFBCTunerNum();
	procInit();
}

int eFBCTunerManager::fe_slot_id(const eDVBRegisteredFrontend *fe) const
{
	return(fe->m_frontend->getSlotID());
}

eFBCTunerManager::~eFBCTunerManager()
{
	isDestroyed = true;
}

void eFBCTunerManager::procInit()
{
	eSmartPtrList<eDVBRegisteredFrontend> &frontends = m_res_mgr->m_frontend;

	/* each FBC set has 8 tuners */
	/* first set: 0-7 */
	/* second set: 8-16 */
	/* first, second frontend is top on a set */

	for (eSmartPtrList<eDVBRegisteredFrontend>::iterator it(frontends.begin()); it != frontends.end(); ++it)
	{
		if (!it->m_frontend->is_FBCTuner())
			continue;

		if (isRootFe(*it))
			setProcFBCID(fe_slot_id(it), getFBCID(fe_slot_id(it)));
	}
}

int eFBCTunerManager::getFBCTunerNum()
{
	char tmp[128];
	int fbc_tuner_num;

	// FIXME
	// This finds the number of virtual tuners even though we want to know
	// the number of physical ("root") tuners. The calculation in the return
	// statement is just a guess and works for now.

	for(fbc_tuner_num = 0; fbc_tuner_num < 128; fbc_tuner_num++)
	{
		snprintf(tmp, sizeof(tmp), "/proc/stb/frontend/%d/fbc_id", fbc_tuner_num);

		if(access(tmp, F_OK))
			break;
	}

	return (fbc_tuner_num / (FBC_TUNER_SET / 2));
}

int eFBCTunerManager::setProcFBCID(int fe_id, int fbc_id)
{
	char filename[128];
	char data[4];
	sprintf(filename, "/proc/stb/frontend/%d/fbc_id", fe_id);
	int fd = open(filename, O_RDWR);
	if(fd < 0) {
		eDebug("[*][eFBCTunerManager::setProcFBCID] open failed, %s: %m", filename);
		return -1;
	}
	else
	{
		if(isLinkedByIndex(fe_id))
			fbc_id += 0x10; // 0x10 : isLinked, 0x01 : fbc_id

		sprintf(data, "%x", fbc_id);
		write(fd, data, strlen(data));
		close(fd);
	}
	return 0;
}

bool eFBCTunerManager::isRootFeSlot(int fe_slot_id)
{
	return (fe_slot_id%FBC_TUNER_SET < m_fbc_tuner_num) ? true : false;
}


bool eFBCTunerManager::isRootFe(eDVBRegisteredFrontend *fe)
{
	return isRootFeSlot(fe_slot_id(fe));
}

bool eFBCTunerManager::isSameFbcSet(int a, int b)
{
	return (a/FBC_TUNER_SET) == (b/FBC_TUNER_SET) ? true : false;
}

bool eFBCTunerManager::isSupportDVBS(eDVBRegisteredFrontend *fe)
{
	return (fe->m_frontend->supportsDeliverySystem(SYS_DVBS, true) || fe->m_frontend->supportsDeliverySystem(SYS_DVBS2, true)) ? true : false;
}

int eFBCTunerManager::getFBCID(int top_fe_id)
{
	return 2*top_fe_id/FBC_TUNER_SET + top_fe_id%FBC_TUNER_SET; /* (0,1,8,9,16,17...) -> (0,1,2,3,4,5...)*/
}

int eFBCTunerManager::setDefaultFBCID(eDVBRegisteredFrontend *fe)
{
	if (!isRootFe(fe))
		return -1;

	return setProcFBCID(fe_slot_id(fe), getFBCID(fe_slot_id(fe)));
}

void eFBCTunerManager::updateFBCID(eDVBRegisteredFrontend *next_fe, eDVBRegisteredFrontend *prev_fe)
{
	eDVBRegisteredFrontend *top_fe = getTop(prev_fe);
	setProcFBCID(fe_slot_id(next_fe), getFBCID(fe_slot_id(top_fe)));
}

eDVBRegisteredFrontend *eFBCTunerManager::getPrev(eDVBRegisteredFrontend *fe)
{
	eDVBRegisteredFrontend *prev_fe = NULL;
	long linked_prev_ptr = -1;
	fe->m_frontend->getData(eDVBFrontend::LINKED_PREV_PTR, linked_prev_ptr);
	if (linked_prev_ptr != -1)
		prev_fe = (eDVBRegisteredFrontend *)linked_prev_ptr;
	return prev_fe;
}

eDVBRegisteredFrontend *eFBCTunerManager::getNext(eDVBRegisteredFrontend *fe)
{
	eDVBRegisteredFrontend *next_fe = NULL;
	long linked_next_ptr = -1;
	fe->m_frontend->getData(eDVBFrontend::LINKED_NEXT_PTR, linked_next_ptr);
	if (linked_next_ptr != -1)
		next_fe = (eDVBRegisteredFrontend *)linked_next_ptr;
	return next_fe;
}

eDVBRegisteredFrontend *eFBCTunerManager::getTop(eDVBRegisteredFrontend *fe)
{
	eDVBRegisteredFrontend *prev_fe = fe;
	long linked_prev_ptr = -1;
	fe->m_frontend->getData(eDVBFrontend::LINKED_PREV_PTR, linked_prev_ptr);
	while(linked_prev_ptr != -1)
	{
		prev_fe = (eDVBRegisteredFrontend *)linked_prev_ptr;
		prev_fe->m_frontend->getData(eDVBFrontend::LINKED_PREV_PTR, linked_prev_ptr);
	}
	return prev_fe;
}

eDVBRegisteredFrontend *eFBCTunerManager::getLast(eDVBRegisteredFrontend *fe)
{
	eDVBRegisteredFrontend *next_fe = fe;
	long linked_next_ptr = -1;
	fe->m_frontend->getData(eDVBFrontend::LINKED_NEXT_PTR, linked_next_ptr);
	while(linked_next_ptr != -1)
	{
		next_fe = (eDVBRegisteredFrontend *)linked_next_ptr;
		next_fe->m_frontend->getData(eDVBFrontend::LINKED_NEXT_PTR, linked_next_ptr);
	}
	return next_fe;
}

bool eFBCTunerManager::isLinked(eDVBRegisteredFrontend *fe)
{
	return getPrev(fe) ? true:false;
}

bool eFBCTunerManager::isLinkedByIndex(int fe_idx)
{
	bool linked = false;
	eSmartPtrList<eDVBRegisteredFrontend> &frontends = m_res_mgr->m_frontend;

	for (eSmartPtrList<eDVBRegisteredFrontend>::iterator it(frontends.begin()); it != frontends.end(); ++it)
	{
		if (fe_slot_id(it) == fe_idx)
		{
			linked = isLinked(*it);
			break;
		}
	}
	return linked;
}

bool eFBCTunerManager::checkTop(eDVBRegisteredFrontend *fe)
{
	return getPrev(fe) ? false:true;
}

int eFBCTunerManager::connectLinkByIndex(int link_fe_index, int prev_fe_index, int next_fe_index, bool simulate)
{
	eSmartPtrList<eDVBRegisteredFrontend> &frontends = simulate ? m_res_mgr->m_simulate_frontend : m_res_mgr->m_frontend;

	eDVBRegisteredFrontend *link_fe=NULL;
	eDVBRegisteredFrontend *prev_fe=NULL;
	eDVBRegisteredFrontend *next_fe=NULL;

	for (eSmartPtrList<eDVBRegisteredFrontend>::iterator it(frontends.begin()); it != frontends.end(); ++it)
	{
		if (fe_slot_id(it) == prev_fe_index)
			prev_fe = *it;
		else if (fe_slot_id(it) == next_fe_index)
			next_fe = *it;
		else if (fe_slot_id(it) == link_fe_index)
			link_fe = *it;
	}

	if (prev_fe && next_fe && link_fe)
	{
		link_fe->m_frontend->setEnabled(true);

		prev_fe->m_frontend->setData(eDVBFrontend::LINKED_NEXT_PTR, (long)link_fe);
		link_fe->m_frontend->setData(eDVBFrontend::LINKED_PREV_PTR, (long)prev_fe);

		link_fe->m_frontend->setData(eDVBFrontend::LINKED_NEXT_PTR, (long)next_fe);
		next_fe->m_frontend->setData(eDVBFrontend::LINKED_PREV_PTR, (long)link_fe);
	}
	else
		return -1;

	return 0;
}

int eFBCTunerManager::connectLinkByIndex(int link_fe_index, int prev_fe_index, bool simulate)
{
	eSmartPtrList<eDVBRegisteredFrontend> &frontends = simulate ? m_res_mgr->m_simulate_frontend : m_res_mgr->m_frontend;

	eDVBRegisteredFrontend *link_fe=NULL;
	eDVBRegisteredFrontend *prev_fe=NULL;

	for (eSmartPtrList<eDVBRegisteredFrontend>::iterator it(frontends.begin()); it != frontends.end(); ++it)
	{
		if (fe_slot_id(it) == prev_fe_index)
			prev_fe = *it;
		else if (fe_slot_id(it) == link_fe_index)
			link_fe = *it;
	}

	if (prev_fe && link_fe)
	{
		link_fe->m_frontend->setEnabled(true);

		prev_fe->m_frontend->setData(eDVBFrontend::LINKED_NEXT_PTR, (long)link_fe);
		link_fe->m_frontend->setData(eDVBFrontend::LINKED_PREV_PTR, (long)prev_fe);
	}
	else
		return -1;

	return 0;
}

int eFBCTunerManager::disconnectLinkByIndex(int link_fe_index, int prev_fe_index, int next_fe_index, bool simulate)
{
	eSmartPtrList<eDVBRegisteredFrontend> &frontends = simulate ? m_res_mgr->m_simulate_frontend : m_res_mgr->m_frontend;

	eDVBRegisteredFrontend *link_fe=NULL;
	eDVBRegisteredFrontend *prev_fe=NULL;
	eDVBRegisteredFrontend *next_fe=NULL;

	for (eSmartPtrList<eDVBRegisteredFrontend>::iterator it(frontends.begin()); it != frontends.end(); ++it)
	{
		if (fe_slot_id(it) == prev_fe_index)
			prev_fe = *it;
		else if (fe_slot_id(it) == next_fe_index)
			next_fe = *it;
		else if (fe_slot_id(it) == link_fe_index)
			link_fe = *it;
	}

	if (prev_fe && next_fe && link_fe)
	{
		prev_fe->m_frontend->setData(eDVBFrontend::LINKED_NEXT_PTR, (long)next_fe);
		next_fe->m_frontend->setData(eDVBFrontend::LINKED_PREV_PTR, (long)prev_fe);

		link_fe->m_frontend->setData(eDVBFrontend::LINKED_PREV_PTR, (long)-1);
		link_fe->m_frontend->setData(eDVBFrontend::LINKED_NEXT_PTR, (long)-1);

		link_fe->m_frontend->setEnabled(false);
	}
	else
		return -1;

	return 0;
}
int eFBCTunerManager::disconnectLinkByIndex(int link_fe_index, int prev_fe_index, bool simulate)
{
	eSmartPtrList<eDVBRegisteredFrontend> &frontends = simulate ? m_res_mgr->m_simulate_frontend : m_res_mgr->m_frontend;

	eDVBRegisteredFrontend *link_fe=NULL;
	eDVBRegisteredFrontend *prev_fe=NULL;

	for (eSmartPtrList<eDVBRegisteredFrontend>::iterator it(frontends.begin()); it != frontends.end(); ++it)
	{
		if (fe_slot_id(it) == prev_fe_index)
			prev_fe = *it;
		else if (fe_slot_id(it) == link_fe_index)
			link_fe = *it;
	}

	if (prev_fe && link_fe)
	{
		prev_fe->m_frontend->setData(eDVBFrontend::LINKED_NEXT_PTR, (long)-1);
		link_fe->m_frontend->setData(eDVBFrontend::LINKED_PREV_PTR, (long)-1);

		link_fe->m_frontend->setEnabled(false);
	}
	else
		return -1;

	return 0;
}

int eFBCTunerManager::connectLink(eDVBRegisteredFrontend *link_fe, eDVBRegisteredFrontend *prev_fe, eDVBRegisteredFrontend *next_fe, bool simulate)
{
	int ret = connectLinkByIndex(fe_slot_id(link_fe), fe_slot_id(prev_fe), fe_slot_id(next_fe), !simulate);

	if(!ret)
	{
		prev_fe->m_frontend->setData(eDVBFrontend::LINKED_NEXT_PTR, (long)link_fe);
		link_fe->m_frontend->setData(eDVBFrontend::LINKED_PREV_PTR, (long)prev_fe);

		link_fe->m_frontend->setData(eDVBFrontend::LINKED_NEXT_PTR, (long)next_fe);
		next_fe->m_frontend->setData(eDVBFrontend::LINKED_PREV_PTR, (long)link_fe);

		link_fe->m_frontend->setEnabled(true);	
	}

	return ret;
}

int eFBCTunerManager::connectLink(eDVBRegisteredFrontend *link_fe, eDVBRegisteredFrontend *prev_fe, bool simulate)
{
	int ret = connectLinkByIndex(fe_slot_id(link_fe), fe_slot_id(prev_fe), !simulate);
	if(!ret)
	{
		prev_fe->m_frontend->setData(eDVBFrontend::LINKED_NEXT_PTR, (long)link_fe);
		link_fe->m_frontend->setData(eDVBFrontend::LINKED_PREV_PTR, (long)prev_fe);

		link_fe->m_frontend->setEnabled(true);
	}

	return ret;
}

int eFBCTunerManager::disconnectLink(eDVBRegisteredFrontend *link_fe, eDVBRegisteredFrontend *prev_fe, eDVBRegisteredFrontend *next_fe, bool simulate)
{
	int ret = disconnectLinkByIndex(fe_slot_id(link_fe), fe_slot_id(prev_fe), fe_slot_id(next_fe), !simulate);
	if(!ret)
	{
		prev_fe->m_frontend->setData(eDVBFrontend::LINKED_NEXT_PTR, (long)next_fe);
		next_fe->m_frontend->setData(eDVBFrontend::LINKED_PREV_PTR, (long)prev_fe);

		link_fe->m_frontend->setData(eDVBFrontend::LINKED_PREV_PTR, (long)-1);
		link_fe->m_frontend->setData(eDVBFrontend::LINKED_NEXT_PTR, (long)-1);

		link_fe->m_frontend->setEnabled(false);
	}

	return ret;
}

int eFBCTunerManager::disconnectLink(eDVBRegisteredFrontend *link_fe, eDVBRegisteredFrontend *prev_fe, bool simulate)
{
	int ret = disconnectLinkByIndex(fe_slot_id(link_fe), fe_slot_id(prev_fe), !simulate);
	if(!ret)
	{
		prev_fe->m_frontend->setData(eDVBFrontend::LINKED_NEXT_PTR, (long)-1);
		link_fe->m_frontend->setData(eDVBFrontend::LINKED_PREV_PTR, (long)-1);

		link_fe->m_frontend->setEnabled(false);
	}

	return ret;
}

void eFBCTunerManager::connectLinkNoSimulate(eDVBRegisteredFrontend *link_fe, eDVBRegisteredFrontend *top_fe)
{
	eDVBRegisteredFrontend *last_fe = getLast(top_fe);

	last_fe->m_frontend->setData(eDVBFrontend::LINKED_NEXT_PTR, (long)link_fe);
	link_fe->m_frontend->setData(eDVBFrontend::LINKED_PREV_PTR, (long)last_fe);

	link_fe->m_frontend->setEnabled(true);

	updateLNBSlotMask(fe_slot_id(link_fe), fe_slot_id(top_fe), false);
}

void eFBCTunerManager::disconnectLinkNoSimulate(eDVBRegisteredFrontend *link_fe)
{
	if(getNext(link_fe))
		return;

	eDVBRegisteredFrontend *prev_fe = getPrev(link_fe);

	if(!prev_fe)
		return;

	prev_fe->m_frontend->setData(eDVBFrontend::LINKED_NEXT_PTR, (long)-1);
	link_fe->m_frontend->setData(eDVBFrontend::LINKED_PREV_PTR, (long)-1);
		
	link_fe->m_frontend->setEnabled(false);

	updateLNBSlotMask(fe_slot_id(link_fe), fe_slot_id(prev_fe), true);
}

bool eFBCTunerManager::checkUsed(eDVBRegisteredFrontend *fe, bool a_simulate)
{
	if (fe->m_inuse > 0)
		return true;

	bool simulate = !a_simulate;

	eSmartPtrList<eDVBRegisteredFrontend> &frontends = simulate ? m_res_mgr->m_simulate_frontend : m_res_mgr->m_frontend;

	for (eSmartPtrList<eDVBRegisteredFrontend>::iterator it(frontends.begin()); it != frontends.end(); ++it)
		if (fe_slot_id(it) == fe_slot_id(fe))
			return (it->m_inuse >0)?true:false;

	return false;
}

bool eFBCTunerManager::canLink(eDVBRegisteredFrontend *fe)
{
	if(isRootFe(fe))
		return false;

	if(getPrev(fe) || getNext(fe))
		return false;

	if(isUnicable(fe))
		return false;

	return true;
}

bool eFBCTunerManager::isUnicable(eDVBRegisteredFrontend *fe)
{
	int slot_idx = fe_slot_id(fe);
	bool is_unicable = false;

	ePtr<eDVBSatelliteEquipmentControl> sec = eDVBSatelliteEquipmentControl::getInstance();
	for (int idx=0; idx <= sec->m_lnbidx; ++idx )
	{
		eDVBSatelliteLNBParameters &lnb_param = sec->m_lnbs[idx];
		if ( lnb_param.m_slot_mask & (1 << slot_idx) )
		{
			is_unicable = lnb_param.SatCR_idx != -1;
			break;
		}
	}
	return is_unicable;
}

int eFBCTunerManager::isCompatibleWith(ePtr<iDVBFrontendParameters> &feparm, eDVBRegisteredFrontend *link_fe, bool simulate)
{
	eDVBRegisteredFrontend *best_fbc_fe;

	return isCompatibleWith(feparm, link_fe, best_fbc_fe, simulate);
}

int eFBCTunerManager::isCompatibleWith(ePtr<iDVBFrontendParameters> &feparm, eDVBRegisteredFrontend *link_fe, eDVBRegisteredFrontend *&fbc_fe, bool simulate)
{
	int best_score = 0;

	eSmartPtrList<eDVBRegisteredFrontend> &frontends = simulate ? m_res_mgr->m_simulate_frontend : m_res_mgr->m_frontend;

	for (eSmartPtrList<eDVBRegisteredFrontend>::iterator it(frontends.begin()); it != frontends.end(); ++it)
	{
		if (!it->m_frontend->is_FBCTuner())
			continue;

		if (!isRootFe(*it))
			continue;

		if(!it->m_frontend->getEnabled())
			continue;

		if(!isSameFbcSet(fe_slot_id(link_fe), fe_slot_id(it)))
			continue;

		if(it->m_inuse == 0) // no link to a fe not in use.
			continue;

		if(isLinked(*it)) // no link to a fe linked to another.
			continue;

		if(isUnicable(*it))
			continue;

		connectLinkNoSimulate(link_fe, *it);

		int c = link_fe->m_frontend->isCompatibleWith(feparm);
		if (c > best_score)
		{
			best_score = c;
			fbc_fe = (eDVBRegisteredFrontend *)*it;
		}

		disconnectLinkNoSimulate(link_fe);
	}

	return best_score;
}

void eFBCTunerManager::connectSortedLink(eDVBRegisteredFrontend *link_fe, eDVBRegisteredFrontend *top_fe, bool simulate)
{
	int link_fe_id = fe_slot_id(link_fe);
	int top_fe_id = fe_slot_id(top_fe);
	int prev_fe_id = link_fe_id - 1;

	if (prev_fe_id < 0)
		return;

	eDVBRegisteredFrontend *next_fe = top_fe;
	long linked_next_ptr = -1;
	top_fe->m_frontend->getData(eDVBFrontend::LINKED_NEXT_PTR, linked_next_ptr);
	while(linked_next_ptr != -1)
	{
		next_fe = (eDVBRegisteredFrontend *)linked_next_ptr;
		next_fe->m_frontend->getData(eDVBFrontend::LINKED_NEXT_PTR, linked_next_ptr);

		if (fe_slot_id(next_fe) == prev_fe_id)
			break;
	}

	eDVBRegisteredFrontend *prev_fe = next_fe;

	next_fe = getNext(prev_fe);	

	if (next_fe)
	{
		int res = connectLink(link_fe, prev_fe, next_fe, simulate);
		if (res)
			return;
	}
	else
	{
		int res = connectLink(link_fe, prev_fe, simulate);
		if (res)
			return;
	}

	setProcFBCID(link_fe_id, getFBCID(top_fe_id));
	updateLNBSlotMask(link_fe_id, top_fe_id, false);
}

void eFBCTunerManager::addLink(eDVBRegisteredFrontend *link_fe, eDVBRegisteredFrontend *top_fe, bool simulate)
{
	if (!isRootFe(top_fe))
		return;

	connectSortedLink(link_fe, top_fe, simulate);
}

/* all unused linked fbc fe must be unlinked! */
void eFBCTunerManager::unset(eDVBRegisteredFrontend *fe)
{
	bool simulate = fe->m_frontend->is_simulate();

	if (isRootFe(fe))
		return;

	if(checkUsed(fe, simulate))
		return;

	if(isUnicable(fe))
		return;

	eDVBRegisteredFrontend *linked_prev_fe = getPrev(fe);
	eDVBRegisteredFrontend *linked_next_fe = getNext(fe);

	if (!linked_prev_fe)
		return;

	if (linked_next_fe)
	{
		int res = disconnectLink(fe, linked_prev_fe, linked_next_fe, simulate);
		if (res)
			return;
	}
	else
	{
		int res = disconnectLink(fe, linked_prev_fe, simulate);
		if (res)
			return;
	}

	updateLNBSlotMask(fe_slot_id(fe), fe_slot_id(linked_prev_fe), true);
}

bool eFBCTunerManager::canAllocateLink(eDVBRegisteredFrontend *fe, bool simulate)
{
	if (!isRootFe(fe))
		return false;

	if (isLinked(fe))
		return false;

	eSmartPtrList<eDVBRegisteredFrontend> &frontends = simulate ? m_res_mgr->m_simulate_frontend : m_res_mgr->m_frontend;

	for (eSmartPtrList<eDVBRegisteredFrontend>::iterator it(frontends.begin()); it != frontends.end(); ++it)
		if (it->m_frontend->is_FBCTuner() && !isRootFe(*it) && isSameFbcSet(fe_slot_id(fe), fe_slot_id(it)) && !it->m_frontend->getEnabled() && !isLinked(*it))
			return true;

	return false;
}

int eFBCTunerManager::updateLNBSlotMask(int dest_slot, int src_slot, bool remove)
{
	ePtr<eDVBSatelliteEquipmentControl> sec = eDVBSatelliteEquipmentControl::getInstance();

	int sec_lnbidx = sec->m_lnbidx;

	int found = 0;
	for (int idx=0; idx <= sec_lnbidx; ++idx )
	{
		eDVBSatelliteLNBParameters &lnb_param = sec->m_lnbs[idx];

		if ( lnb_param.m_slot_mask & (1 << src_slot) )
		{
			if (!remove)
				lnb_param.m_slot_mask |= (1 << dest_slot);
			else
				lnb_param.m_slot_mask &= ~(1 << dest_slot);

			found = 1;
		}
	}

	return 0;
}

int eFBCTunerManager::getLinkedSlotID(int fe_id)
{
	int link = -1;
	eSmartPtrList<eDVBRegisteredFrontend> &frontends = m_res_mgr->m_frontend;
	for (eSmartPtrList<eDVBRegisteredFrontend>::iterator it(frontends.begin()); it != frontends.end(); ++it)
	{
		if(it->m_frontend->getSlotID() == fe_id)
		{
			long prev_ptr = -1;
			it->m_frontend->getData(eDVBFrontend::LINKED_PREV_PTR, prev_ptr);
			if (prev_ptr != -1)
			{
				eDVBRegisteredFrontend *prev_fe = (eDVBRegisteredFrontend *)prev_ptr;
				link = fe_slot_id(prev_fe);
			}
			break;
		}
	}

	return link;
}
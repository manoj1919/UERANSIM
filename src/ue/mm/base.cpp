//
// This file is a part of UERANSIM open source project.
// Copyright (c) 2021 ALİ GÜNGÖR.
//
// The software and all associated files are licensed under GPL-3.0
// and subject to the terms and conditions defined in LICENSE file.
//

#include "mm.hpp"

#include <nas/utils.hpp>
#include <ue/app/task.hpp>
#include <ue/nas/task.hpp>
#include <ue/rrc/task.hpp>
#include <utils/common.hpp>

namespace nr::ue
{

NasMm::NasMm(TaskBase *base, UeTimers *timers) : m_base{base}, m_timers{timers}, m_sm{nullptr}
{
    m_logger = base->logBase->makeUniqueLogger(base->config->getLoggerPrefix() + "nas");

    m_rmState = ERmState::RM_DEREGISTERED;
    m_cmState = ECmState::CM_IDLE;
    m_mmState = EMmState::MM_DEREGISTERED;
    m_mmSubState = EMmSubState::MM_DEREGISTERED_NA;
    m_uState = E5UState::U1_UPDATED;
    m_autoBehaviour = base->config->autoBehaviour;
    m_validSim = base->config->supi.has_value();
}

void NasMm::onStart(NasSm *sm)
{
    m_sm = sm;
}

void NasMm::onQuit()
{
    // TODO
}

void NasMm::triggerMmCycle()
{
    m_base->nasTask->push(new NwUeNasToNas(NwUeNasToNas::PERFORM_MM_CYCLE));
}

void NasMm::performMmCycle()
{
    if (m_mmState == EMmState::MM_NULL)
        return;

    if (m_mmSubState == EMmSubState::MM_DEREGISTERED_NA)
    {
        if (m_validSim)
        {
            if (m_cmState == ECmState::CM_IDLE)
                switchMmState(EMmState::MM_DEREGISTERED, EMmSubState::MM_DEREGISTERED_PLMN_SEARCH);
            else
                switchMmState(EMmState::MM_DEREGISTERED, EMmSubState::MM_DEREGISTERED_NORMAL_SERVICE);
        }
        else
        {
            switchMmState(EMmState::MM_DEREGISTERED, EMmSubState::MM_DEREGISTERED_NO_SUPI);
        }
    }

    if (m_mmSubState == EMmSubState::MM_DEREGISTERED_PLMN_SEARCH ||
        m_mmSubState == EMmSubState::MM_DEREGISTERED_NO_CELL_AVAILABLE ||
        m_mmSubState == EMmSubState::MM_REGISTERED_NO_CELL_AVAILABLE)
    {
        long current = utils::CurrentTimeMillis();
        long elapsedMs = current - m_lastPlmnSearchTrigger;
        if (elapsedMs > 50)
        {
            m_base->rrcTask->push(new NwUeNasToRrc(NwUeNasToRrc::PLMN_SEARCH_REQUEST));
            m_lastPlmnSearchTrigger = current;
        }
        return;
    }

    if (m_mmSubState == EMmSubState::MM_DEREGISTERED_NORMAL_SERVICE)
    {
        if (m_autoBehaviour && !m_timers->t3346.isRunning())
            sendRegistration(nas::ERegistrationType::INITIAL_REGISTRATION, nas::EFollowOnRequest::FOR_PENDING);
        return;
    }

    if (m_mmState == EMmState::MM_REGISTERED_INITIATED)
        return;
    if (m_mmSubState == EMmSubState::MM_REGISTERED_NORMAL_SERVICE)
        return;
    if (m_mmState == EMmState::MM_DEREGISTERED_INITIATED)
        return;
    if (m_mmSubState == EMmSubState::MM_DEREGISTERED_NA)
        return;
    if (m_mmSubState == EMmSubState::MM_DEREGISTERED_NO_SUPI)
        return;

    if (m_autoBehaviour)
    {
        m_logger->err("unhandled UE MM state");
        return;
    }
}

void NasMm::switchMmState(EMmState state, EMmSubState subState)
{
    EMmState oldState = m_mmState;
    EMmSubState oldSubState = m_mmSubState;

    m_mmState = state;
    m_mmSubState = subState;

    onSwitchMmState(oldState, m_mmState, oldSubState, m_mmSubState);

    if (m_base->nodeListener)
    {
        m_base->nodeListener->onSwitch(app::NodeType::UE, m_base->config->getNodeName(), app::StateType::MM,
                                       ToJson(oldSubState).str(), ToJson(subState).str());
        m_base->nodeListener->onSwitch(app::NodeType::UE, m_base->config->getNodeName(), app::StateType::MM_SUB,
                                       ToJson(oldState).str(), ToJson(state).str());
    }

    if (state != oldState || subState != oldSubState)
        m_logger->info("UE switches to state: %s", ToJson(subState).str().c_str());

    triggerMmCycle();
}

void NasMm::switchRmState(ERmState state)
{
    ERmState oldState = m_rmState;
    m_rmState = state;

    onSwitchRmState(oldState, m_rmState);

    if (m_base->nodeListener)
    {
        m_base->nodeListener->onSwitch(app::NodeType::UE, m_base->config->getNodeName(), app::StateType::RM,
                                       ToJson(oldState).str(), ToJson(m_rmState).str());
    }

    // No need to log it
    // m_logger->info("UE switches to state: %s", RmStateName(state));

    triggerMmCycle();
}

void NasMm::switchCmState(ECmState state)
{
    ECmState oldState = m_cmState;
    m_cmState = state;

    onSwitchCmState(oldState, m_cmState);

    if (m_base->nodeListener)
    {
        m_base->nodeListener->onSwitch(app::NodeType::UE, m_base->config->getNodeName(), app::StateType::CM,
                                       ToJson(oldState).str(), ToJson(m_cmState).str());
    }

    if (state != oldState)
        m_logger->info("UE switches to state: %s", ToJson(state).str().c_str());

    triggerMmCycle();
}

void NasMm::switchUState(E5UState state)
{
    E5UState oldState = m_uState;
    m_uState = state;

    onSwitchUState(oldState, m_uState);

    if (m_base->nodeListener)
    {
        m_base->nodeListener->onSwitch(app::NodeType::UE, m_base->config->getNodeName(), app::StateType::U5,
                                       ToJson(oldState).str(), ToJson(m_uState).str());
    }

    if (state != oldState)
        m_logger->info("UE switches to state: %s", ToJson(state).str().c_str());

    triggerMmCycle();
}

void NasMm::onSwitchMmState(EMmState oldState, EMmState newState, EMmSubState oldSubState, EMmSubState newSubSate)
{
    // The UE shall mark the 5G NAS security context on the USIM or in the non-volatile memory as invalid when the UE
    // initiates an initial registration procedure as described in subclause 5.5.1.2 or when the UE leaves state
    // 5GMM-DEREGISTERED for any other state except 5GMM-NULL.
    if (oldState == EMmState::MM_DEREGISTERED && newState != EMmState::MM_DEREGISTERED && newState != EMmState::MM_NULL)
    {
        if (m_currentNsCtx.has_value() || m_nonCurrentNsCtx.has_value())
        {
            m_logger->debug("Deleting NAS security context");

            m_currentNsCtx = {};
            m_nonCurrentNsCtx = {};
        }
    }
}

void NasMm::onSwitchRmState(ERmState oldState, ERmState newState)
{
}

void NasMm::onSwitchCmState(ECmState oldState, ECmState newState)
{
    if (oldState == ECmState::CM_CONNECTED && newState == ECmState::CM_IDLE)
    {
        // 5.5.2.2.6 Abnormal cases in the UE (in de-registration)
        if (m_mmState == EMmState::MM_DEREGISTERED_INITIATED)
        {
            // The de-registration procedure shall be aborted and the UE proceeds as follows:
            // if the de-registration procedure was performed due to disabling of 5GS services, the UE shall enter the
            // 5GMM-NULL state;
            if (m_lastDeregDueToDisable5g)
                switchMmState(EMmState::MM_NULL, EMmSubState::MM_NULL_NA);
            // if the de-registration type "normal de-registration" was requested for reasons other than disabling of
            // 5GS services, the UE shall enter the 5GMM-DEREGISTERED state.
            else if (m_lastDeregistrationRequest->deRegistrationType.switchOff ==
                     nas::ESwitchOff::NORMAL_DE_REGISTRATION)
                switchMmState(EMmState::MM_DEREGISTERED, EMmSubState::MM_DEREGISTERED_NA);

            m_lastDeregistrationRequest = nullptr;
            m_lastDeregDueToDisable5g = false;
        }
    }
}

void NasMm::onSwitchUState(E5UState oldState, E5UState newState)
{
}

void NasMm::onTimerExpire(nas::NasTimer &timer)
{
    switch (timer.getCode())
    {
    case 3346: {
        if (m_autoBehaviour && m_mmSubState == EMmSubState::MM_DEREGISTERED_NORMAL_SERVICE)
        {
            sendRegistration(nas::ERegistrationType::INITIAL_REGISTRATION, nas::EFollowOnRequest::FOR_PENDING);
        }
        break;
    }
    case 3512: {
        if (m_autoBehaviour && m_mmState == EMmState::MM_REGISTERED && m_cmState == ECmState::CM_CONNECTED)
        {
            sendRegistration(nas::ERegistrationType::PERIODIC_REGISTRATION_UPDATING,
                             nas::EFollowOnRequest::FOR_PENDING);
        }
        break;
    }
    case 3521: {
        if (timer.getExpiryCount() == 5)
        {
            timer.resetExpiryCount();
            if (m_mmState == EMmState::MM_DEREGISTERED_INITIATED && m_lastDeregistrationRequest != nullptr)
            {
                m_logger->debug("De-registration aborted");

                if (m_lastDeregDueToDisable5g)
                    switchMmState(EMmState::MM_NULL, EMmSubState::MM_NULL_NA);
                else if (m_lastDeregistrationRequest->deRegistrationType.switchOff ==
                         nas::ESwitchOff::NORMAL_DE_REGISTRATION)
                    switchMmState(EMmState::MM_DEREGISTERED, EMmSubState::MM_DEREGISTERED_NA);
            }
        }
        else
        {
            if (m_mmState == EMmState::MM_DEREGISTERED_INITIATED && m_lastDeregistrationRequest != nullptr)
            {
                m_logger->debug("Retrying de-registration request");

                sendNasMessage(*m_lastDeregistrationRequest);
                m_timers->t3521.start(false);
            }
        }
        break;
    }
    }
}

void NasMm::invalidateAcquiredParams()
{
    m_storedGuti = {};
    m_lastVisitedRegisteredTai = {};
    m_taiList = {};
    m_currentNsCtx = {};
    m_nonCurrentNsCtx = {};
}

void NasMm::invalidateSim()
{
    m_logger->warn("USIM is removed or invalidated");
    m_validSim = false;
    invalidateAcquiredParams();
}

} // namespace nr::ue

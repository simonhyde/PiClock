#include "tally.h"



std::shared_ptr<TallyState> SimpleTallyState::SetLabel(const std::string & label) const
{
    //If it's identical, then return empty pointer
    if((!m_label && label.empty())
            ||(m_label && (label == *m_label)))
        return std::shared_ptr<TallyState>();
    std::shared_ptr<TallyState> ret = std::make_shared<SimpleTallyState>(*this);
    auto derived = dynamic_cast<SimpleTallyState *>(ret.get());
    if(!label.empty())
        derived->m_label = std::make_shared<std::string>(label);
    else
        derived->m_label = std::shared_ptr<std::string>();
    return ret;
}

bool SimpleTallyState::Equals(std::shared_ptr<TallyState> other) const
{
    if(!other)
        return false;
    if(typeid(*other) != typeid(*this))
        return false;
    auto derived = dynamic_cast<SimpleTallyState *>(other.get());
    return derived != NULL 
        && derived->m_FG->Equals(*m_FG)
        && derived->m_BG->Equals(*m_BG)
        && *(derived->m_text) == *m_text;
}

SimpleTallyState::SimpleTallyState(const std::shared_ptr<ClockMsg_SetTally> &pMsg, const std::shared_ptr<TallyState> & pOld)
    :SimpleTallyState(*pMsg, pOld)
{
}

SimpleTallyState::SimpleTallyState(const ClockMsg_SetTally &msg, const std::shared_ptr<TallyState> & pOld)
    :SimpleTallyState(msg.colFg, msg.colBg, msg.sText, pOld)
{
}

SimpleTallyState::SimpleTallyState(const std::string &fg, const std::string &bg, const std::string &_text)
    :m_FG(std::make_shared<TallyColour>(fg)),m_BG(std::make_shared<TallyColour>(bg)),m_text(std::make_shared<std::string>(_text))
{}
SimpleTallyState::SimpleTallyState(const TallyColour & fg, const TallyColour &bg, const std::string &_text)
    :m_FG(std::make_shared<TallyColour>(fg)),m_BG(std::make_shared<TallyColour>(bg)),m_text(std::make_shared<std::string>(_text))
{}

SimpleTallyState::SimpleTallyState(const std::string &fg, const std::string &bg, const std::string &_text, const std::shared_ptr<TallyState> &_old)
    :m_FG(std::make_shared<TallyColour>(fg)),m_BG(std::make_shared<TallyColour>(bg)),m_text(std::make_shared<std::string>(_text))
{
    auto derived = dynamic_cast<SimpleTallyState *>(_old.get());
    if(derived != NULL)
        m_label = derived->m_label;
    else
        m_label = std::shared_ptr<std::string>();
}
SimpleTallyState::SimpleTallyState(const TallyColour & fg, const TallyColour &bg, const std::string &_text, const std::shared_ptr<TallyState> &_old)
    :m_FG(std::make_shared<TallyColour>(fg)),m_BG(std::make_shared<TallyColour>(bg)),m_text(std::make_shared<std::string>(_text))
{
    auto derived = dynamic_cast<SimpleTallyState *>(_old.get());
    if(derived != NULL)
        m_label = derived->m_label;
    else
        m_label = std::shared_ptr<std::string>();
}


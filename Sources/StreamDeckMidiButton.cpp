//==============================================================================
/**
@file       MidiButton.cpp

@brief      MIDI plugin

@copyright  (c) 2020, Clarion Music Ltd
			This source code is licensed under the MIT-style license found in the LICENSE file.
            Parts of this plugin (c) 2019 Fred Emmott, (c) 2018 Corsair Memory, Inc

**/
//==============================================================================

#include "StreamDeckMidiButton.h"
#include "Common/ESDConnectionManager.h"
#include "Common/EPLJSONUtils.h"

#define Message(x) {mConnectionManager->LogMessage(x);}
#define DebugMessage(x) { if (mGlobalSettings->printDebug) mConnectionManager->LogMessage(x);}

//using this for debugging
inline std::string const BoolToString(bool b)
{
  return b ? "true" : "false";
}

template<typename T>
std::string intToHex(T i)
{
  std::stringstream stream;
  stream << "0x"
         << std::setfill ('0') << std::setw(sizeof(T)*2)
         << std::hex << i;
  return stream.str();
}

StreamDeckMidiButton::StreamDeckMidiButton()
{
    //instantiate a new GlobalSettings object
    mGlobalSettings = new GlobalSettings;
    try
    {
        if (midiOut != nullptr)
        {
            delete midiOut;
        }
        midiOut = new RtMidiOut();
        if (midiIn != nullptr)
        {
            delete midiIn;
        }
        midiIn = new RtMidiIn();
    }
    catch (std::exception e)
    {
        exit(EXIT_FAILURE);
    }
    
    //start the timer with 1ms resolution
    eTimer = new Timer(std::chrono::milliseconds(mGlobalSettings->sampleInterval));
    
    //add an event to the timer and use it to update the fades and check for midi input
    auto mTimerUpdate = eTimer->set_interval(std::chrono::milliseconds(mGlobalSettings->sampleInterval), [this]() {this->UpdateTimer(); this->GetMidiInput();});
}

StreamDeckMidiButton::~StreamDeckMidiButton()
{
    try
    {
        if (midiOut != nullptr)
        {
            delete midiOut;
            midiOut = nullptr;
        }
        if (midiIn != nullptr)
        {
            delete midiIn;
            midiIn = nullptr;
        }
        if(mGlobalSettings != nullptr)
        {
            delete mGlobalSettings;
            mGlobalSettings = nullptr;
        }
    }
    catch (std::exception e)
    {
        mConnectionManager->LogMessage(e.what());
        exit(EXIT_FAILURE);
    }
}

bool StreamDeckMidiButton::InitialiseMidi(Direction direction)
{
    switch (direction)
    {
        case Direction::OUT:
        {
            //mConnectionManager->LogMessage("bool MidiButton::InitialiseMidi(Direction OUT)");
            Message("bool MidiButton::InitialiseMidi(Direction OUT)");
            try
            {
                if (midiOut != nullptr)
                {
                    Message("bool MidiButton::InitialiseMidi(Direction OUT): midiOut is already initialised - deleting");
                    delete midiOut;
                    Message("bool MidiButton::InitialiseMidi(Direction OUT): creating a new midiOut");
                    midiOut = new RtMidiOut();
                }
#if defined (__APPLE__)
                if (mGlobalSettings->useVirtualPort)
                {
                    if (mGlobalSettings->printDebug) Message("bool MidiButton::InitialiseMidi(Direction OUT): opening virtual OUTPUT port called: " + mGlobalSettings->portName);
                    midiOut->openVirtualPort(mGlobalSettings->portName);
                }
                else
                {
#endif
                    //idiot check - are there any OUTPUT ports?
                    if (midiOut->getPortCount() == 0)
                    {
                        Message("bool MidiButton::InitialiseMidi(Direction OUT): no midi OUT device available");
                        return false;
                    }
                    else
                    {
                        if (mGlobalSettings->selectedOutPortName == midiOut->getPortName(mGlobalSettings->selectedOutPortIndex))
                        {
                            //the portname and index match - open the port
                            if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("bool MidiButton::InitialiseMidi(Direction OUT): portName and portIndex match - opening OUTPUT port index: " + std::to_string(mGlobalSettings->selectedOutPortIndex) + " called: " + midiOut->getPortName(mGlobalSettings->selectedOutPortIndex));
                            midiOut->openPort(mGlobalSettings->selectedOutPortIndex);
                        }
                        else if (mGlobalSettings->selectedOutPortName != midiOut->getPortName(mGlobalSettings->selectedOutPortIndex))
                        {
                            mConnectionManager->LogMessage("bool MidiButton::InitialiseMidi(Direction OUT): selectedOutPortName and selectedOutPortIndex DON'T match - setting selectedOutPortIndex to the correct name");
                            for (int i = 0; i < midiOut->getPortCount(); i++)
                            {
                                if (midiOut->getPortName(i) == mGlobalSettings->selectedOutPortName) mGlobalSettings->selectedOutPortIndex = i;
                            }
                            if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("bool MidiButton::InitialiseMidi(Direction OUT): opening OUTPUT port index: " + std::to_string(mGlobalSettings->selectedOutPortIndex) + " called: " + midiOut->getPortName(mGlobalSettings->selectedOutPortIndex));
                            midiOut->openPort(mGlobalSettings->selectedOutPortIndex);
                        }
                        else
                        {
                            mConnectionManager->LogMessage("bool MidiButton::InitialiseMidi(Direction OUT): selectedOutPortIndex is out of range - something has gone wrong");
                            return false;
                        }
                    }
#if defined (__APPLE__)
                }
#endif
            }
            catch (RtMidiError &error)
            {
                mConnectionManager->LogMessage("bool MidiButton::InitialiseMidi(Direction OUT): problem with RtMidi - exiting with prejudice");
                mConnectionManager->LogMessage(error.getMessage());
                exit(EXIT_FAILURE);
            }
        }
        case Direction::IN:
        {
            mConnectionManager->LogMessage("bool MidiButton::InitialiseMidi(Direction IN)");
            
            midiUpdateMutex.lock();
            try
            {
                if (midiIn != nullptr)
                {
                    mConnectionManager->LogMessage("bool MidiButton::InitialiseMidi(Direction IN): midiIn is already initialised - deleting");
                    delete midiIn;
                    mConnectionManager->LogMessage("bool MidiButton::InitialiseMidi(Direction IN): creating a new midiIn");
                    midiIn = new RtMidiIn();
                }
                
                // don't ignore sysex, timing or active sensing messages
                midiIn->ignoreTypes(false, false, false);
#if defined (__APPLE__)
                if (mGlobalSettings->useVirtualPort)
                {
                    if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("bool MidiButton::InitialiseMidi(Direction IN): opening virtual INPUT port called: " + mGlobalSettings->portName);
                    midiIn->openVirtualPort(mGlobalSettings->portName);
                }
                else
                {
#endif
                    //idiot check - are there any INPUT ports?
                    if (midiIn->getPortCount() == 0)
                    {
                        mConnectionManager->LogMessage("bool MidiButton::InitialiseMidi(Direction IN): no midi IN device available");
                        midiUpdateMutex.unlock();
                        return false;
                    }
                    else
                    {
                        if (mGlobalSettings->selectedInPortName == midiIn->getPortName(mGlobalSettings->selectedInPortIndex))
                        {
                            //the portname and index match - open the port
                            if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("bool MidiButton::InitialiseMidi(Direction IN): portName and portIndex match - opening INPUT port index: " + std::to_string(mGlobalSettings->selectedInPortIndex) + " called: " + midiIn->getPortName(mGlobalSettings->selectedInPortIndex));
                            midiIn->openPort(mGlobalSettings->selectedInPortIndex);
                        }
                        else if (mGlobalSettings->selectedInPortName != midiIn->getPortName(mGlobalSettings->selectedInPortIndex))
                        {
                            mConnectionManager->LogMessage("bool MidiButton::InitialiseMidi(Direction IN): selectedInPortName and selectedInPortIndex DON'T match - setting selectedInPortIndex to the correct name");
                            for (int i = 0; i < midiIn->getPortCount(); i++)
                            {
                                if (midiIn->getPortName(i) == mGlobalSettings->selectedInPortName) mGlobalSettings->selectedInPortIndex = i;
                            }
                            if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("bool MidiButton::InitialiseMidi(Direction IN): opening INPUT port index: " + std::to_string(mGlobalSettings->selectedInPortIndex) + " called: " + midiIn->getPortName(mGlobalSettings->selectedInPortIndex));
                            midiIn->openPort(mGlobalSettings->selectedInPortIndex);
                        }
                        else
                        {
                            mConnectionManager->LogMessage("bool MidiButton::InitialiseMidi(Direction IN): selectedInPortIndex is out of range - something has gone wrong");
                            midiUpdateMutex.unlock();
                            return false;
                        }
                    }
                }
            }
            catch (RtMidiError &error)
            {
                mConnectionManager->LogMessage("bool MidiButton::InitialiseMidi(Direction IN): problem with RtMidi - exiting with prejudice");
                mConnectionManager->LogMessage(error.getMessage());
                exit(EXIT_FAILURE);
            }
            midiUpdateMutex.unlock();
            return true;
#if defined (__APPLE__)
        }
#endif
    }
}

void StreamDeckMidiButton::GetMidiInput()
{
    //
    // Warning: UpdateTimer() is running in the timer thread
    //
    
    //deal with midi input
    
    midiUpdateMutex.lock();
    
    if (midiIn != nullptr)
    {
        if (mConnectionManager != nullptr)
        {
            std::vector<unsigned char> message;
            std::string logMessage;
            double stamp;
            int nBytes;
            
            stamp = midiIn->getMessage(&message);
            nBytes = message.size();
            if (nBytes > 0)
            {
                mConnectionManager->LogMessage("received a midi message with " + std::to_string(nBytes) + " bytes");
                /*for (int i = 0; i < nBytes; i++)
                {
                    mConnectionManager->LogMessage("Byte " + std::to_string(i) + " = " + std::to_string((int)message[i]) + ", stamp = " + std::to_string(stamp));
                    if ((int)message[i] == 247) mConnectionManager->LogMessage("Byte " + std::to_string(i) + " = " + intToHex((int)message[i]) + " -> EOM");
                    else mConnectionManager->LogMessage("Byte " + std::to_string(i) + " = " + intToHex((int)message[i]));
                }*/

                std::map<std::string, ButtonSettings>::iterator storedButtonSettingsIterator = storedButtonSettings.begin();
                
                while (storedButtonSettingsIterator != storedButtonSettings.end())
                {
                    //DebugMessage("void StreamDeckMidiButton::GetMidiInput()", "void StreamDeckMidiButton::GetMidiInput(): storedButtonSettings status byte for button " + storedButtonSettingsIterator->first + " is " + std::to_string(storedButtonSettings[storedButtonSettingsIterator->first].statusByte) + " and data byte 1 " + std::to_string(storedButtonSettings[storedButtonSettingsIterator->first].dataByte1));
                    //Message("storedButtonSettings status byte for button " + storedButtonSettingsIterator->first + " is " + std::to_string(storedButtonSettings[storedButtonSettingsIterator->first].statusByte));
                    //Message("storedButtonSettings data byte 1 for button " + storedButtonSettingsIterator->first + " is " + std::to_string(storedButtonSettings[storedButtonSettingsIterator->first].dataByte1));
                    if (storedButtonSettings[storedButtonSettingsIterator->first].statusByte == (int)message[0])//matching status byte
                    {
                        if (storedButtonSettings[storedButtonSettingsIterator->first].dataByte1 > 0 && storedButtonSettings[storedButtonSettingsIterator->first].dataByte1 == (int)message[1])//matching data byte 1
                        {
                            if (mGlobalSettings->printDebug) Message("storedButtonSettings status byte for button " + storedButtonSettingsIterator->first + " is " + std::to_string(storedButtonSettings[storedButtonSettingsIterator->first].statusByte) + " which matches incoming status byte of " + std::to_string((int)message[0]) + " and data byte 1 of " + std::to_string((int)message[1]));

                            if (storedButtonSettings[storedButtonSettingsIterator->first].inAction == SEND_NOTE_ON_TOGGLE)
                            {
                                storedButtonSettings[storedButtonSettingsIterator->first].state = !storedButtonSettings[storedButtonSettingsIterator->first].state;
                                ChangeButtonState(storedButtonSettingsIterator->first);
                            }
                            else if (storedButtonSettings[storedButtonSettingsIterator->first].inAction == SEND_CC_TOGGLE)
                            {
                                if (storedButtonSettings[storedButtonSettingsIterator->first].dataByte2 == (int)message[2])//incoming message matches the main CC value selected
                                {
                                    storedButtonSettings[storedButtonSettingsIterator->first].state = 0;
                                    ChangeButtonState(storedButtonSettingsIterator->first);
                                }
                                else if (storedButtonSettings[storedButtonSettingsIterator->first].dataByte2Alt == (int)message[2])//incoming message matches the alternate CC value selected)
                                {
                                    storedButtonSettings[storedButtonSettingsIterator->first].state = 1;
                                    ChangeButtonState(storedButtonSettingsIterator->first);
                                }
                            }
                        }
                    }
                    storedButtonSettingsIterator++;
                }
            }
        }
    }
    midiUpdateMutex.unlock();
}

void StreamDeckMidiButton::UpdateTimer()
{
    //check each button and see if we need to do a fade
    // Create a map iterator and point to beginning of map
    
    std::map<std::string, FadeSet>::iterator it = this->storedFadeSettings.begin();
    
    // Iterate over the map using Iterator till end.
    while (it != this->storedFadeSettings.end())
    {
        if (this->storedFadeSettings[it->first].fadeActive)//it->first is the same as inContext
        {
            if (this->storedFadeSettings[it->first].UpdateFade())
            {
                //we have an updated value - send it out as a MIDI CC message
                mConnectionManager->LogMessage("Trying to send a midi message with status byte " + std::to_string(storedButtonSettings[it->first].statusByte) + ", data byte 1 " + std::to_string(storedButtonSettings[it->first].dataByte1) + " & data byte 2 ALT " + std::to_string(storedFadeSettings[it->first].currentValue));
                midiMessageMutex.lock();
                midiMessage.clear();
                midiMessage.push_back(storedButtonSettings[it->first].statusByte);
                midiMessage.push_back(storedButtonSettings[it->first].dataByte1);
                midiMessage.push_back(storedFadeSettings[it->first].currentValue);
                SendMidiMessage(midiMessage);
                midiMessageMutex.unlock();
            }
        }
        if (this->storedFadeSettings[it->first].fadeFinished)
        {
            //we have a finished fade - print a TICK to the button
            mConnectionManager->ShowOKForContext(it->first);
            this->storedFadeSettings[it->first].fadeFinished = !this->storedFadeSettings[it->first].fadeFinished;
        }
        // Increment the Iterator to point to next entry
        it++;
    }
}

void StreamDeckMidiButton::StoreButtonSettings(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
    mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings()");
    if (mGlobalSettings->printDebug)
    {
        mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): received button settings for " + inContext + ", with action " + inAction + ": " + inPayload.dump().c_str());
    }
    
    ButtonSettings thisButtonSettings;
    
    //should iterate through the JSON payload here and store everything into a struct
    
    thisButtonSettings.inAction = inAction;
    thisButtonSettings.inContext = inContext;
    
    //STATUS BYTE
    if (inPayload["settings"].find("statusByte") != inPayload["settings"].end())
    {
        thisButtonSettings.statusByte = inPayload["settings"]["statusByte"];
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): Setting statusByte to " + std::to_string(thisButtonSettings.statusByte));
    }
    
    //COMMON
    if (inPayload["settings"].find("dataByte1") != inPayload["settings"].end())
    {
        thisButtonSettings.dataByte1 = inPayload["settings"]["dataByte1"];
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): Setting dataByte1 to " + std::to_string(thisButtonSettings.dataByte1));
    }
    if (inPayload["settings"].find("dataByte2") != inPayload["settings"].end())
    {
        thisButtonSettings.dataByte2 = inPayload["settings"]["dataByte2"];
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): Setting dataByte2 to " + std::to_string(thisButtonSettings.dataByte2));
    }
    if (inPayload["settings"].find("dataByte2Alt") != inPayload["settings"].end())
    {
        thisButtonSettings.dataByte2Alt = inPayload["settings"]["dataByte2Alt"];
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): Setting dataByte2Alt to " + std::to_string(thisButtonSettings.dataByte2Alt));
    }
    if (inPayload["settings"].find("dataByte5") != inPayload["settings"].end())
    {
        thisButtonSettings.dataByte5 = inPayload["settings"]["dataByte5"];
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): Setting dataByte5 to " + std::to_string(thisButtonSettings.dataByte5));
    }
    
    //NOTE OFF mode
    if (inPayload["settings"].find("noteOffMode") != inPayload["settings"].end())
    {
        thisButtonSettings.noteOffMode = inPayload["settings"]["noteOffMode"];
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): Setting noteOffMode to " + std::to_string(thisButtonSettings.noteOffMode));
    }

    //VARIOUS
    if (inPayload["settings"].find("toggleFade") != inPayload["settings"].end())
    {
        thisButtonSettings.toggleFade = inPayload["settings"]["toggleFade"];
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): Setting toggleFade to " + BoolToString(thisButtonSettings.toggleFade));
    }
    if (inPayload["settings"].find("fadeTime") != inPayload["settings"].end())
    {
        thisButtonSettings.fadeTime = inPayload["settings"]["fadeTime"];
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): Setting fadeTime to " + std::to_string(thisButtonSettings.fadeTime));
    }
    if (inPayload["settings"].find("fadeCurve") != inPayload["settings"].end())
    {
        thisButtonSettings.fadeCurve = inPayload["settings"]["fadeCurve"];
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): Setting fadeCurve to " + std::to_string(thisButtonSettings.fadeCurve));
    }
    if (inPayload["settings"].find("ccMode") != inPayload["settings"].end())
    {
        thisButtonSettings.ccMode = inPayload["settings"]["ccMode"];
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): Setting ccMode to " + std::to_string(thisButtonSettings.ccMode));
        //if ccMode is 2 or 3 set the toggleFade flag
        if (thisButtonSettings.ccMode == 2 || thisButtonSettings.ccMode == 3)
        {
            thisButtonSettings.toggleFade = true;
            if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): ccMode is 2 or 3 - setting toggleFade to " + BoolToString(thisButtonSettings.toggleFade));
        }
        else
        {
            thisButtonSettings.toggleFade = false;
            if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): ccMode is 0 or 1 - setting toggleFade to " + BoolToString(thisButtonSettings.toggleFade));
        }
    }
    
    //store everything into the map
    if (storedButtonSettings.insert(std::make_pair(inContext, thisButtonSettings)).second == false)
    {
        //key already exists, so replace it
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): Key already exists - replacing");
        storedButtonSettings[inContext] = thisButtonSettings;
    }
    
    // NEED TO CHANGE THIS DEBUG SECTION
    if (mGlobalSettings->printDebug)//check it's been stored correctly
    {
        //iterate through settings and log messages - should be a way to do this in a loop
        if (inAction == SEND_NOTE_ON || SEND_NOTE_ON_TOGGLE)
        {
            mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): midiChannel for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].statusByte - NOTE_ON));
            mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): dataByte1 for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].dataByte1));
            mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): dataByte2 for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].dataByte2));
            if (inAction == SEND_NOTE_ON) mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): noteOffMode for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].noteOffMode));
        }
        else if (inAction == SEND_CC || SEND_CC_TOGGLE)
        {
            mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): midiChannel for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].statusByte - CC));
            mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): dataByte1 for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].dataByte1));
            mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): dataByte2 for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].dataByte2));
            if (inAction == SEND_CC_TOGGLE) mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): dataByte2Alt for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].dataByte2Alt));
            mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): toggleFade for " + inContext + " is set to: " + BoolToString(storedButtonSettings[inContext].toggleFade));
            if (storedButtonSettings[inContext].toggleFade)
            {
                mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): fadeTime for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].fadeTime));
                mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): fadeCurve for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].fadeCurve));
            }
        }
        else if (inAction == SEND_PROGRAM_CHANGE)
        {
            mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): midiChannel for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].statusByte - PC));
            mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): dataByte1 for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].dataByte1));
        }
        else if (inAction == SEND_MMC)
        {
            mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): dataByte5 for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].dataByte5));
        }
    }

    //if it's an MMC button set the icon
    if (inAction == SEND_MMC)
    {
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): inAction == SEND_MMC -> SetActionIcon()");
        SetActionIcon(inAction, inContext, inPayload, inDeviceID);
    }
    
    //if it's a CC Toggle button generate the fadeSet, if required
    if (storedButtonSettings[inContext].toggleFade)//create the fadeSet for the button
    {
        if (storedButtonSettings[inContext].fadeTime == 0)
        {
            if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): We have a fadeTime of 0! - divide by zero error, so ignoring by switching toggleFade off");
            storedButtonSettings[inContext].toggleFade = false; //to avoid divide by zero problem
        }
        else
        {
            if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): Generating the fade lookup table");
            //FadeSet thisButtonFadeSet(storedButtonSettings[inContext].midiValue, storedButtonSettings[inContext].midiValueSec, storedButtonSettings[inContext].fadeTime, storedButtonSettings[inContext].fadeCurve, mGlobalSettings->sampleInterval);
            FadeSet thisButtonFadeSet(storedButtonSettings[inContext].dataByte2, storedButtonSettings[inContext].dataByte2Alt, storedButtonSettings[inContext].fadeTime, storedButtonSettings[inContext].fadeCurve, mGlobalSettings->sampleInterval);
            if (storedFadeSettings.insert(std::make_pair(inContext, thisButtonFadeSet)).second == false)
            {
                //key already exists, so replace it
                if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::StoreButtonSettings(): FadeSet already exists - replacing");
                storedFadeSettings[inContext] = thisButtonFadeSet;
            }
            switch (thisButtonSettings.ccMode)
            {
                case 0: case 1://single CC value, or momentary without fade
                    break;
                case 2://fade IN until button is released, and then fade IN - KeyUpForAction()
                    storedFadeSettings[inContext].currentDirection = Direction::IN;
                    break;
                case 3://fade OUT until button is released, and then fade IN - KeyUpForAction()
                    storedFadeSettings[inContext].currentDirection = Direction::OUT;
                    break;
            }
        }
    }
}

void StreamDeckMidiButton::KeyDownForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
    mConnectionManager->LogMessage("void MidiButton::KeyDownForAction()");
        
    if (mGlobalSettings->printDebug)
    {
        mConnectionManager->LogMessage("void MidiButton::KeyDownForAction(): dumping the JSON payload: " + inPayload.dump());
        //mConnectionManager->LogMessage(inPayload.dump().c_str());//REALLY USEFUL for debugging
    }
    
    if (storedButtonSettings.find(inContext) == storedButtonSettings.end())//something's gone wrong - no settings stored, so the PI probably hasn't been opened
    {
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::KeyDownForAction(): No storedButtonSettings - something went wrong");
        StoreButtonSettings(inAction, inContext, inPayload, inDeviceID);
    }

    try
    {
        if (inAction == SEND_NOTE_ON)
        {
            if (mGlobalSettings->printDebug)
            {
                mConnectionManager->LogMessage("void MidiButton::KeyDownForAction(): inAction: " + inAction);
                mConnectionManager->LogMessage("void MidiButton::KeyDownForAction(): inContext: " + inContext);
            }
            midiMessageMutex.lock();
            midiMessage.clear();
            midiMessage.push_back(storedButtonSettings[inContext].statusByte);
            midiMessage.push_back(storedButtonSettings[inContext].dataByte1);
            midiMessage.push_back(storedButtonSettings[inContext].dataByte2);
            SendMidiMessage(midiMessage);
            midiMessageMutex.unlock();
            
            switch (storedButtonSettings[inContext].noteOffMode)
            {
                case 0:
                {//no note off message
                    if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::SendNoteOn(noteOffMode = 0): no note off message");
                    break;
                }
                case PUSH_NOTE_OFF:
                {//1: {//send note off message immediately
                    if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::SendNoteOn(noteOffMode = 1): send note off immediately");
                    
                    //send a note on message with velocity 0 - same thing
                    midiMessageMutex.lock();
                    midiMessage.clear();
                    midiMessage.push_back(storedButtonSettings[inContext].statusByte);
                    midiMessage.push_back(storedButtonSettings[inContext].dataByte1);
                    midiMessage.push_back(0);
                    SendMidiMessage(midiMessage);
                    midiMessageMutex.unlock();
                    break;
                }
                case RELEASE_NOTE_OFF:
                {//2: {//send note off on KeyUp
                    if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::SendNoteOn(noteOffMode = 2): send note off on KeyUp");
                    break;
                }
                default:
                {
                    break;
                }
            }
        }
        else if (inAction == SEND_NOTE_ON_TOGGLE)
        {
            if (mGlobalSettings->printDebug)
            {
                mConnectionManager->LogMessage("void MidiButton::KeyDownForAction(): inAction: " + inAction);
                mConnectionManager->LogMessage("void MidiButton::KeyDownForAction(): inContext: " + inContext);
                if (inPayload.contains("state")) mConnectionManager->LogMessage("void MidiButton::KeyDownForAction(): we have a state of " + std::to_string(inPayload["state"].get<int>()));
                if (inPayload.contains("userDesiredState")) mConnectionManager->LogMessage("void MidiButton::KeyDownForAction(): we have a userDesiredState of " + std::to_string(inPayload["userDesiredState"].get<int>()));
            }
            if (inPayload.contains("state"))
            {
                if (inPayload.contains("userDesiredState"))
                {
                    if (inPayload["userDesiredState"].get<int>() == 0)
                    {
                        midiMessageMutex.lock();
                        midiMessage.clear();
                        midiMessage.push_back(storedButtonSettings[inContext].statusByte);
                        midiMessage.push_back(storedButtonSettings[inContext].dataByte1);
                        midiMessage.push_back(storedButtonSettings[inContext].dataByte2);
                        SendMidiMessage(midiMessage);
                        midiMessageMutex.unlock();
                        storedButtonSettings[inContext].state = 0;
                    }
                    else if (inPayload["userDesiredState"].get<int>() == 1)
                    {
                        //send a note on message with velocity 0 - same thing
                        midiMessageMutex.lock();
                        midiMessage.clear();
                        midiMessage.push_back(storedButtonSettings[inContext].statusByte);
                        midiMessage.push_back(storedButtonSettings[inContext].dataByte1);
                        midiMessage.push_back(0);
                        SendMidiMessage(midiMessage);
                        midiMessageMutex.unlock();
                        storedButtonSettings[inContext].state = 1;
                    }
                }
                else
                {
                    if (inPayload["state"].get<int>() == 0)
                    {
                        midiMessageMutex.lock();
                        midiMessage.clear();
                        midiMessage.push_back(storedButtonSettings[inContext].statusByte);
                        midiMessage.push_back(storedButtonSettings[inContext].dataByte1);
                        midiMessage.push_back(storedButtonSettings[inContext].dataByte2);
                        SendMidiMessage(midiMessage);
                        midiMessageMutex.unlock();
                        storedButtonSettings[inContext].state = 0;
                    }
                    else if (inPayload["state"].get<int>() == 1)
                    {
                        //send a note on message with velocity 0 - same thing
                        midiMessageMutex.lock();
                        midiMessage.clear();
                        midiMessage.push_back(storedButtonSettings[inContext].statusByte);
                        midiMessage.push_back(storedButtonSettings[inContext].dataByte1);
                        midiMessage.push_back(0);
                        SendMidiMessage(midiMessage);
                        midiMessageMutex.unlock();
                        storedButtonSettings[inContext].state = 1;
                    }
                }
            }
            else mConnectionManager->LogMessage("void MidiButton::KeyDownForAction(): something went wrong - should have a state, and we don't have");
        }
        else if (inAction == SEND_CC)
        {
            if (mGlobalSettings->printDebug)
            {
                mConnectionManager->LogMessage("void MidiButton::KeyDownForAction(): inAction: " + inAction);
                mConnectionManager->LogMessage("void MidiButton::KeyDownForAction(): inContext: " + inContext);
            }
            switch (storedButtonSettings[inContext].ccMode)
            {
                case 0: case 1://single CC value, or momentary without fade
                    midiMessageMutex.lock();
                    midiMessage.push_back(storedButtonSettings[inContext].statusByte);
                    midiMessage.push_back(storedButtonSettings[inContext].dataByte1);
                    midiMessage.push_back(storedButtonSettings[inContext].dataByte2);
                    SendMidiMessage(midiMessage);
                    midiMessageMutex.unlock();
                    break;
                case 2: case 3:
                    storedFadeSettings[inContext].FadeButtonPressed();
                    break;
            }
        }
        else if (inAction == SEND_CC_TOGGLE)
        {
            if (mGlobalSettings->printDebug)
            {
                mConnectionManager->LogMessage("void MidiButton::KeyDownForAction(): inAction: " + inAction);
                mConnectionManager->LogMessage("void MidiButton::KeyDownForAction(): inContext: " + inContext);
                if (inPayload.contains("state")) mConnectionManager->LogMessage("void MidiButton::KeyDownForAction(): we have a state of " + std::to_string(inPayload["state"].get<int>()));
                if (inPayload.contains("userDesiredState")) mConnectionManager->LogMessage("void MidiButton::KeyDownForAction(): we have a userDesiredState of " + std::to_string(inPayload["userDesiredState"].get<int>()));
            }
            if (inPayload.contains("state"))
            {
                if (inPayload.contains("userDesiredState"))
                {
                    if (inPayload["userDesiredState"].get<int>() == 0)
                    {
                        if (!storedButtonSettings[inContext].toggleFade)
                        {
                            mConnectionManager->LogMessage("Trying to send a midi message with status byte " + std::to_string(storedButtonSettings[inContext].statusByte) + ", data byte 1 " + std::to_string(storedButtonSettings[inContext].dataByte1) + " & data byte 2 " + std::to_string(storedButtonSettings[inContext].dataByte2));
                            midiMessageMutex.lock();
                            midiMessage.clear();
                            midiMessage.push_back(storedButtonSettings[inContext].statusByte);
                            midiMessage.push_back(storedButtonSettings[inContext].dataByte1);
                            midiMessage.push_back(storedButtonSettings[inContext].dataByte2);
                            SendMidiMessage(midiMessage);
                            midiMessageMutex.unlock();
                            storedButtonSettings[inContext].state = 0;
                        }
                            
                        else if (storedButtonSettings[inContext].toggleFade)
                        {
                            if (storedFadeSettings[inContext].fadeActive) storedFadeSettings[inContext].FadeButtonPressed();
                            else
                            {
                                storedFadeSettings[inContext].currentDirection = Direction::IN;
                                storedFadeSettings[inContext].fadeActive = true;
                            }
                            storedFadeSettings[inContext].FadeButtonPressed();
                        }
                    }
                    else if (inPayload["userDesiredState"].get<int>() == 1)
                    {
                        if (!storedButtonSettings[inContext].toggleFade)
                        {
                            mConnectionManager->LogMessage("Trying to send a midi message with status byte " + std::to_string(storedButtonSettings[inContext].statusByte) + ", data byte 1 " + std::to_string(storedButtonSettings[inContext].dataByte1) + " & data byte 2 ALT " + std::to_string(storedButtonSettings[inContext].dataByte2Alt));
                            midiMessageMutex.lock();
                            midiMessage.clear();
                            midiMessage.push_back(storedButtonSettings[inContext].statusByte);
                            midiMessage.push_back(storedButtonSettings[inContext].dataByte1);
                            midiMessage.push_back(storedButtonSettings[inContext].dataByte2Alt);
                            SendMidiMessage(midiMessage);
                            midiMessageMutex.unlock();
                            storedButtonSettings[inContext].state = 1;
                        }
                        else if (storedButtonSettings[inContext].toggleFade)
                        {
                            if (storedFadeSettings[inContext].fadeActive) storedFadeSettings[inContext].FadeButtonPressed();
                            else
                            {
                                storedFadeSettings[inContext].currentDirection = Direction::OUT;
                                storedFadeSettings[inContext].fadeActive = true;
                            }
                            storedFadeSettings[inContext].FadeButtonPressed();
                        }
                    }
                }
                else
                {
                    if (inPayload["state"].get<int>() == 0)
                    {
                        if (!storedButtonSettings[inContext].toggleFade)
                        {
                            mConnectionManager->LogMessage("Trying to send a midi message with status byte " + std::to_string(storedButtonSettings[inContext].statusByte) + ", data byte 1 " + std::to_string(storedButtonSettings[inContext].dataByte1) + " & data byte 2 " + std::to_string(storedButtonSettings[inContext].dataByte2));
                            midiMessageMutex.lock();
                            midiMessage.clear();
                            midiMessage.push_back(storedButtonSettings[inContext].statusByte);
                            midiMessage.push_back(storedButtonSettings[inContext].dataByte1);
                            midiMessage.push_back(storedButtonSettings[inContext].dataByte2);
                            SendMidiMessage(midiMessage);
                            midiMessageMutex.unlock();
                            storedButtonSettings[inContext].state = 0;
                        }
                        else if (storedButtonSettings[inContext].toggleFade)
                        {
                            if (storedFadeSettings[inContext].fadeActive) storedFadeSettings[inContext].FadeButtonPressed();
                            else
                            {
                                storedFadeSettings[inContext].currentDirection = Direction::IN;
                                storedFadeSettings[inContext].fadeActive = true;
                            }
                        }
                    }
                    else if (inPayload["state"].get<int>() == 1)
                    {
                        if (!storedButtonSettings[inContext].toggleFade)
                        {
                            mConnectionManager->LogMessage("Trying to send a midi message with status byte " + std::to_string(storedButtonSettings[inContext].statusByte) + ", data byte 1 " + std::to_string(storedButtonSettings[inContext].dataByte1) + " & data byte 2 ALT " + std::to_string(storedButtonSettings[inContext].dataByte2Alt));
                            midiMessageMutex.lock();
                            midiMessage.clear();
                            midiMessage.push_back(storedButtonSettings[inContext].statusByte);
                            midiMessage.push_back(storedButtonSettings[inContext].dataByte1);
                            midiMessage.push_back(storedButtonSettings[inContext].dataByte2Alt);
                            SendMidiMessage(midiMessage);
                            midiMessageMutex.unlock();
                            storedButtonSettings[inContext].state = 1;
                        }
                        else if (storedButtonSettings[inContext].toggleFade)
                        {
                            if (storedFadeSettings[inContext].fadeActive) storedFadeSettings[inContext].FadeButtonPressed();
                            else
                            {
                                storedFadeSettings[inContext].currentDirection = Direction::OUT;
                                storedFadeSettings[inContext].fadeActive = true;
                            }
                        }
                    }
                }
            }
            else mConnectionManager->LogMessage("void MidiButton::KeyDownForAction(): Something's gone wrong - should have a state, and we don't have");
        }
        else if (inAction == SEND_PROGRAM_CHANGE)
        {
            if (mGlobalSettings->printDebug) mConnectionManager->LogMessage(inAction);
            midiMessageMutex.lock();
            midiMessage.clear();
            midiMessage.push_back(storedButtonSettings[inContext].statusByte);
            midiMessage.push_back(storedButtonSettings[inContext].dataByte1);
            SendMidiMessage(midiMessage);
            midiMessageMutex.unlock();
        }
        else if (inAction == SEND_MMC)
        {
            if (mGlobalSettings->printDebug) mConnectionManager->LogMessage(inAction);
            midiMessageMutex.lock();
            midiMessage.clear();
            midiMessage.push_back(240); //F0 hex
            midiMessage.push_back(127); //7F hex
            midiMessage.push_back(127); //7F hex - device-id all devices
            midiMessage.push_back(6); //6 - sub-id #1 command
            midiMessage.push_back(storedButtonSettings[inContext].dataByte5); //send the MMC command
            midiMessage.push_back(247); //F7 - message end
            SendMidiMessage(midiMessage);
            midiMessageMutex.unlock();
        }
        else
        {
            //something has gone wrong
            mConnectionManager->LogMessage("void MidiButton::KeyDownForAction(): Something's gone wrong - no inAction sent, which shouldn't be possible");
        }
    }
    catch (std::exception e)
    {
        mConnectionManager->LogMessage("void MidiButton::KeyDownForAction(): Something's gone wrong - an unknown error has occurred as below");
        mConnectionManager->LogMessage(e.what());
    }
}

void StreamDeckMidiButton::KeyUpForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
    mConnectionManager->LogMessage("void MidiButton::KeyUpForAction()");
    try
    {
        if (inAction == SEND_NOTE_ON)
        {
            if (storedButtonSettings[inContext].noteOffMode == 2)//(noteOffMode == 2)
            {
                if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::KeyUpForAction(): send note off");
                midiMessageMutex.lock();
                midiMessage.clear();
                midiMessage.push_back(storedButtonSettings[inContext].statusByte);
                midiMessage.push_back(storedButtonSettings[inContext].dataByte1);
                midiMessage.push_back(0);
                SendMidiMessage(midiMessage);
                midiMessageMutex.unlock();

                //SendNoteOff(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiNote, storedButtonSettings[inContext].midiVelocity);
            }
        }
        else if (inAction == SEND_CC)
        {
            switch (storedButtonSettings[inContext].ccMode)
            {
                case 0://single CC value only
                    break;
                case 1://momentary without fade
                    if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::KeyUpForAction(): send secondary CC value");
                    midiMessageMutex.lock();
                    midiMessage.clear();
                    midiMessage.push_back(storedButtonSettings[inContext].statusByte);
                    midiMessage.push_back(storedButtonSettings[inContext].dataByte1);
                    midiMessage.push_back(storedButtonSettings[inContext].dataByte2Alt);
                    SendMidiMessage(midiMessage);
                    midiMessageMutex.unlock();
                    break;
                case 2: case 3://fade OUT after fading IN
                    if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::KeyUpForAction(): ReverseFade()");
                    if (storedFadeSettings[inContext].fadeActive) storedFadeSettings[inContext].ReverseFade();
                    else if (!storedFadeSettings[inContext].fadeActive) storedFadeSettings[inContext].fadeActive = true;
                    break;
            }
        }
        else return;
    }
    catch (std::exception e)
    {
        mConnectionManager->LogMessage("void MidiButton::KeyUpForAction(): something went wrong - error message follows");
        mConnectionManager->LogMessage(e.what());
    }
}

void StreamDeckMidiButton::WillAppearForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
    mConnectionManager->LogMessage("void MidiButton::WillAppearForAction()");
    mConnectionManager->GetGlobalSettings();

    //lock the context
    mVisibleContextsMutex.lock();
    mVisibleContexts.insert(inContext);
    
    //store the button settings
    if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::WillAppearForAction(): setting the storedButtonSettings for button: " + inContext);
    StoreButtonSettings(inAction, inContext, inPayload, inDeviceID);
    
    //unlock the context
    mVisibleContextsMutex.unlock();
}

void StreamDeckMidiButton::WillDisappearForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
    //remove the context
	mVisibleContextsMutex.lock();
	mVisibleContexts.erase(inContext);
	mVisibleContextsMutex.unlock();
}

void StreamDeckMidiButton::DeviceDidConnect(const std::string& inDeviceID, const json &inDeviceInfo)
{
    mConnectionManager->LogMessage("void MidiButton::DeviceDidConnect()");
}

void StreamDeckMidiButton::DeviceDidDisconnect(const std::string& inDeviceID)
{
	// Nothing to do
}

void StreamDeckMidiButton::ChangeButtonState(const std::string& inContext)
{
    if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::ChangeButtonState(): inContext" + inContext + ", and required state " + std::to_string(storedButtonSettings[inContext].state));
    else mConnectionManager->LogMessage("void MidiButton::ChangeButtonState()");
    mConnectionManager->SetState(storedButtonSettings[inContext].state, inContext);
}

void StreamDeckMidiButton::SendToPlugin(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
    if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::SendToPlugin(): " + inPayload.dump());
    else mConnectionManager->LogMessage("void MidiButton::SendToPlugin()");
    
    if (inAction == SEND_MMC)
    {
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::SendToPlugin(): new MMC action - set the action icon");
        SetActionIcon(inAction, inContext, inPayload, inDeviceID);
    }
    
    const auto event = EPLJSONUtils::GetStringByName(inPayload, "event");
    if (event == "getMidiPorts")
    {
        std::map <std::string, std::string> midiPortList = GetMidiPortList(Direction::OUT);
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::SendToPlugin(): get list of MIDI OUT ports - " + json({{"event", "midiOutPorts"},{"midiOutPortList", midiPortList}}).dump());
        mConnectionManager->SendToPropertyInspector(inAction, inContext,json({{"event", "midiOutPorts"},{"midiOutPortList", midiPortList}}));
        
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::SendToPlugin(): selected MIDI OUT port - " + json({{"event", "midiOutPortSelected"},{"midiOutPortSelected", mGlobalSettings->selectedOutPortIndex}}).dump());
        mConnectionManager->SendToPropertyInspector(inAction, inContext,json({{"event", "midiOutPortSelected"},{"midiOutPortSelected", mGlobalSettings->selectedOutPortIndex}}));
        
        midiPortList = GetMidiPortList(Direction::IN);
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::SendToPlugin(): get list of MIDI IN ports - " + json({{"event", "midiInPorts"},{"midiInPortList", midiPortList}}).dump());
        mConnectionManager->SendToPropertyInspector(inAction, inContext,json({{"event", "midiInPorts"},{"midiInPortList", midiPortList}}));
        
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage(json({{"event", "midiInPortSelected"},{"midiInPortSelected", mGlobalSettings->selectedInPortIndex}}).dump().c_str());
        mConnectionManager->SendToPropertyInspector(inAction, inContext,json({{"event", "midiInPortSelected"},{"midiInPortSelected", mGlobalSettings->selectedInPortIndex}}));
        return;
    }
    //else something's gone wrong
    else mConnectionManager->LogMessage("void MidiButton::SendToPlugin(): something went wrong - not expecting this message to be sent. Dumping payload: " + inPayload.dump());
}

void StreamDeckMidiButton::DidReceiveGlobalSettings(const json& inPayload)
{
    mConnectionManager->LogMessage("void MidiButton::DidReceiveGlobalSettings(): inPayload = " + inPayload.dump());
    
    //boolean to say settings have changed
    bool midiSettingsChanged = false;
    
    //check to see that the payload has the necessary information, and set the midiSettingsChanged flag if so
    
    //see if the printDebug flag has been set
    if (inPayload["settings"].find("printDebug") != inPayload["settings"].end())
    {
        if (mGlobalSettings->printDebug != inPayload["settings"]["printDebug"])//printDebug has changed
        {
            mGlobalSettings->printDebug = inPayload["settings"]["printDebug"];
            if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::DidReceiveGlobalSettings(): mGlobalSettings->printDebug is set to " + BoolToString(mGlobalSettings->printDebug) + " - will print all debug messages");
            else if (!mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::DidReceiveGlobalSettings(): mGlobalSettings->printDebug is set to " + BoolToString(mGlobalSettings->printDebug) + " - won't print verbose debug messages");
        }
    }
    //see if the virtual port flag has been set
    if (inPayload["settings"].find("useVirtualPort") != inPayload["settings"].end())
    {
        if (mGlobalSettings->useVirtualPort != inPayload["settings"]["useVirtualPort"])//useVirtualPort flag has changed
        {
            Message("void MidiButton::DidReceiveGlobalSettings(): useVirtualPort has changed");
            mGlobalSettings->useVirtualPort = inPayload["settings"]["useVirtualPort"];//update the useVirtualPort flag
            midiSettingsChanged = true;
        }
    }
    //get the portName, if that's been set
    if (inPayload["settings"].find("portName") != inPayload["settings"].end())
    {
        std::string portName = mGlobalSettings->portName;
        if (portName.compare(inPayload["settings"]["portName"]) != 0)//portName has changed
        {
            Message("void MidiButton::DidReceiveGlobalSettings(): portName has changed");
            mGlobalSettings->portName = inPayload["settings"]["portName"];
            midiSettingsChanged = true;
        }
    }
    //get the selectedPortIndexes, if set
    if (inPayload["settings"].find("selectedOutPortIndex") != inPayload["settings"].end())
    {
        if (mGlobalSettings->selectedOutPortIndex != inPayload["settings"]["selectedOutPortIndex"])
        {
            Message("void MidiButton::DidReceiveGlobalSettings(): selectedOutPortIndex has changed");
            mGlobalSettings->selectedOutPortIndex = inPayload["settings"]["selectedOutPortIndex"];
            midiSettingsChanged = true;
        }
    }
    if (inPayload["settings"].find("selectedInPortIndex") != inPayload["settings"].end())
    {
        if (mGlobalSettings->selectedInPortIndex != inPayload["settings"]["selectedInPortIndex"])
        {
            Message("void MidiButton::DidReceiveGlobalSettings(): selectedInPortIndex has changed");
            mGlobalSettings->selectedInPortIndex = inPayload["settings"]["selectedInPortIndex"];
            midiSettingsChanged = true;
        }
    }
    //get the selectedPortNames, if set
    if (inPayload["settings"].find("selectedOutPortName") != inPayload["settings"].end())
    {
        std::string selectedOutPortName = mGlobalSettings->selectedOutPortName;
        if (selectedOutPortName.compare(inPayload["settings"]["selectedOutPortName"]) != 0)//portName has changed
        {
            Message("void MidiButton::DidReceiveGlobalSettings(): selectedOutPortName has changed");
            mGlobalSettings->selectedOutPortName = inPayload["settings"]["selectedOutPortName"];
            midiSettingsChanged = true;
        }
    }
    if (inPayload["settings"].find("selectedInPortName") != inPayload["settings"].end())
    {
        std::string selectedInPortName = mGlobalSettings->selectedInPortName;
        if (selectedInPortName.compare(inPayload["settings"]["selectedInPortName"]) != 0)//portName has changed
        {
            Message("void MidiButton::DidReceiveGlobalSettings(): selectedInPortName has changed");
            mGlobalSettings->selectedInPortName = inPayload["settings"]["selectedInPortName"];
            midiSettingsChanged = true;
        }
    }

    if (!midiSettingsChanged) return;//nothing has changed (except possibly the printDebug flag - return
    else if (midiSettingsChanged)
    {
        if (mGlobalSettings->useVirtualPort)
        {
            if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::DidReceiveGlobalSettings(): opening the virtual port with portName " + mGlobalSettings->portName);
            if (InitialiseMidi(Direction::IN))
            {
                mConnectionManager->LogMessage("void MidiButton::DidReceiveGlobalSettings(): MIDI input initialised successfully");
            }
            else
            {
                mConnectionManager->LogMessage("void MidiButton::DidReceiveGlobalSettings(): something went wrong initialising MIDI input");
                return;
            }
            if (InitialiseMidi(Direction::OUT))
            {
                mConnectionManager->LogMessage("void MidiButton::DidReceiveGlobalSettings(): MIDI output initialised successfully");
            }
            else
            {
                mConnectionManager->LogMessage("void MidiButton::DidReceiveGlobalSettings(): something went wrong initialising MIDI output");
                return;
            }
        }
        else if (!mGlobalSettings->useVirtualPort)
        {
            if (mGlobalSettings->printDebug)
            {
            mConnectionManager->LogMessage("void MidiButton::DidReceiveGlobalSettings(): opening the physical OUTPUT port with index " + std::to_string(mGlobalSettings->selectedOutPortIndex) + " & port name " + midiOut->getPortName(mGlobalSettings->selectedOutPortIndex));
            mConnectionManager->LogMessage("void MidiButton::DidReceiveGlobalSettings(): opening the physical INPUT port with index " + std::to_string(mGlobalSettings->selectedInPortIndex) + " & port name " + midiOut->getPortName(mGlobalSettings->selectedInPortIndex));
            }
            if (InitialiseMidi(Direction::IN))
            {
                mConnectionManager->LogMessage("void MidiButton::DidReceiveGlobalSettings(): MIDI input initialised successfully");
            }
            else
            {
                mConnectionManager->LogMessage("void MidiButton::DidReceiveGlobalSettings(): something went wrong initialising MIDI input");
                return;
            }
            if (InitialiseMidi(Direction::OUT))
            {
                mConnectionManager->LogMessage("void MidiButton::DidReceiveGlobalSettings(): MIDI output initialised successfully");
            }
            else
            {
                mConnectionManager->LogMessage("void MidiButton::DidReceiveGlobalSettings(): something went wrong initialising MIDI output");
                return;
            }
        }
    }
}

void StreamDeckMidiButton::DidReceiveSettings(const std::string& inAction, const std::string& inContext, const json& inPayload, const std::string& inDeviceID)
{
    mConnectionManager->LogMessage("void MidiButton::DidReceiveSettings()");
    if (mGlobalSettings->printDebug)
    {
        mConnectionManager->LogMessage("void MidiButton::DidReceiveSettings(): StoreButtonSettings() for button: " + inContext + " with settings: " + inPayload.dump().c_str());
    }
    StoreButtonSettings(inAction, inContext, inPayload, inDeviceID);
}

void StreamDeckMidiButton::SendMidiMessage(const std::vector<unsigned char>& midiMessage)
{
    if (!mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::SendMidiMessage()");
    //else if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void MidiButton::SendNoteOn(Channel: " + std::to_string(midiChannel) + ", Note: " + std::to_string(midiNote) + ", Velocity: " + std::to_string(midiVelocity) + ", Note Off Mode: " + std::to_string(noteOffMode) + ")");
    midiOut->sendMessage(&midiMessage);
}

void StreamDeckMidiButton::SetActionIcon(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
    mConnectionManager->LogMessage("void MidiButton::SetActionIcon()");
    //get the MMC message
    try
    {
        switch (storedButtonSettings[inContext].dataByte5)
        {
            case 1: //STOP
                if (storedButtonSettings[inContext].midiMMCIsActive) mConnectionManager->SetImage("data:image/png;base64," + GetPngAsString("./Icons/stop_active.png"), inContext, 0);
                else mConnectionManager->SetImage("data:image/png;base64," + GetPngAsString("./Icons/stop_inactive.png"), inContext, 0);
                break;
            case 2: //PLAY
                if (storedButtonSettings[inContext].midiMMCIsActive) mConnectionManager->SetImage("data:image/png;base64," + GetPngAsString("./Icons/play_active.png"), inContext, 0);
                else mConnectionManager->SetImage("data:image/png;base64," + GetPngAsString("./Icons/play_inactive.png"), inContext, 0);
                break;
            case 4://FFWD
                if (storedButtonSettings[inContext].midiMMCIsActive) mConnectionManager->SetImage("data:image/png;base64," + GetPngAsString("./Icons/fastforward_active.png"), inContext, 0);
                else mConnectionManager->SetImage("data:image/png;base64," + GetPngAsString("./Icons/fastforward_inactive.png"), inContext, 0);
                break;
            case 5://RWD
                if (storedButtonSettings[inContext].midiMMCIsActive) mConnectionManager->SetImage("data:image/png;base64," + GetPngAsString("./Icons/fastrewind_active.png"), inContext, 0);
                else mConnectionManager->SetImage("data:image/png;base64," + GetPngAsString("./Icons/fastrewind_inactive.png"), inContext, 0);
                break;
            case 6://RECORD
                if (storedButtonSettings[inContext].midiMMCIsActive) mConnectionManager->SetImage("data:image/png;base64," + GetPngAsString("./Icons/record_active.png"), inContext, 0);
                else mConnectionManager->SetImage("data:image/png;base64," + GetPngAsString("./Icons/record_inactive.png"), inContext, 0);
                break;
            case 9://PAUSE
                if (storedButtonSettings[inContext].midiMMCIsActive) mConnectionManager->SetImage("data:image/png;base64," + GetPngAsString("./Icons/pause_active.png"), inContext, 0);
                else mConnectionManager->SetImage("data:image/png;base64," + GetPngAsString("./Icons/pause_inactive.png"), inContext, 0);
                break;
            default:
                break;
        }
    }
    catch (...)
    {
        mConnectionManager->LogMessage("void MidiButton::SetActionIcon(): something went wrong setting the action icon - the file is probably missing/unreadable");
    }
}

std::map <std::string, std::string> StreamDeckMidiButton::GetMidiPortList(Direction direction)
{
    switch (direction)
    {
        case Direction::OUT:
        {
            mConnectionManager->LogMessage("std::map <std::string, std::string> MidiButton::GetMidiPortList(Direction OUT)");
            std::map <std::string, std::string> midiPortList;
            std::string portName;
            unsigned int nPorts = midiOut->getPortCount();
            if (nPorts == 0)
            {
                midiPortList.insert(std::make_pair("ERROR - no MIDI output port available", std::to_string(0)));
                return midiPortList;
            }
            if (nPorts == 1)
            {
                midiPortList.insert(std::make_pair(portName, std::to_string(1)));
                return midiPortList;
            }
            else
            {
                int count = 0;
                for (unsigned int i = 0; i < nPorts; i++)
                {
                    portName = midiOut->getPortName(i);
                    midiPortList.insert(std::make_pair(portName, std::to_string(i)));
                    count++;
                }
            }
            
            if (mGlobalSettings->printDebug)
            {
                std::map<std::string, std::string>::iterator it = midiPortList.begin();
                while(it != midiPortList.end())
                {
                    mConnectionManager->LogMessage("std::map <std::string, std::string> MidiButton::GetMidiPortList(Direction OUT): " + it->first + " has port index: " + it->second);
                    it++;
                }
            }
            return midiPortList;
        }
        case Direction::IN:
        {
            mConnectionManager->LogMessage("std::map <std::string, std::string> MidiButton::GetMidiPortList(Direction IN)");
            std::map <std::string, std::string> midiPortList;
            std::string portName;
            unsigned int nPorts = midiIn->getPortCount();
            if (nPorts == 0)
            {
                midiPortList.insert(std::make_pair("ERROR - no MIDI input port available", std::to_string(0)));
                return midiPortList;
            }
            if (nPorts == 1)
            {
                midiPortList.insert(std::make_pair(portName, std::to_string(1)));
                return midiPortList;
            }
            else
            {
                int count = 0;
                for (unsigned int i = 0; i < nPorts; i++)
                {
                    portName = midiIn->getPortName(i);
                    midiPortList.insert(std::make_pair(portName, std::to_string(i)));
                    count++;
                }
            }
            
            if (mGlobalSettings->printDebug)
            {
                std::map<std::string, std::string>::iterator it = midiPortList.begin();
                while(it != midiPortList.end())
                {
                    mConnectionManager->LogMessage("std::map <std::string, std::string> MidiButton::GetMidiPortList(Direction IN): " + it->first + " has port index: " + it->second);
                    it++;
                }
            }
            return midiPortList;
        }
    }
}

std::string StreamDeckMidiButton::GetPngAsString(const char* filename)
{
    mConnectionManager->LogMessage("std::string MidiButton::GetPngAsString()");
    //try reading the file
    std::vector<unsigned char> imageFile = ReadFile(filename);
    if (imageFile.empty())
    {
        mConnectionManager->LogMessage("std::string MidiButton::GetPngAsString(): empty image file");
        return NULL;
    }
    else
    {
        std::string imageFileAsString(imageFile.begin(), imageFile.end());
        return base64_encode(reinterpret_cast<const unsigned char*>(imageFileAsString.c_str()), imageFileAsString.length());
    }
}

std::vector<unsigned char> StreamDeckMidiButton::ReadFile(const char* filename)
{
    mConnectionManager->LogMessage("std::vector<unsigned char> MidiButton::ReadFile()");
    // open the file:
    std::ifstream file(filename, std::ios::binary);
    std::vector<unsigned char> vec;
    
    if (!filename)
    {
        mConnectionManager->LogMessage("std::vector<unsigned char> MidiButton::ReadFile(): couldn't open file " + std::string(filename));
        return vec;
    }
    // Stop eating new lines in binary mode!!!
    file.unsetf(std::ios::skipws);

    // get its size:
    std::streampos fileSize;

    file.seekg(0, std::ios::end);
    fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // reserve capacity
    
    vec.reserve(fileSize);

    // read the data:
    vec.insert(vec.begin(),
               std::istream_iterator<unsigned char>(file),
               std::istream_iterator<unsigned char>());

    return vec;
}

StreamDeckMidiButton::FadeSet::FadeSet()
{

}

StreamDeckMidiButton::FadeSet::FadeSet(const int fromValue, const int toValue, const float fadeTime, const float fadeCurve, const int sampleInterval)
{
    //set up the initial values
    this->fromValue = fromValue;
    this->toValue = toValue;
    this->setSize = (fadeTime * (1000 / sampleInterval));
    this->intervalSize = (fadeTime/setSize);
    
    //calculate the look-up tables
    if (fadeCurve == 0)
    {
        // create a vector of length setSize
        std::vector<float> set(setSize);
        
        // now assign the values to the vector
        for (int i = 0; i < setSize + 1; i++)
            {
                 set[i] = (float)fromValue + i * ( ((float)toValue - (float)fromValue) / setSize);
            }
        this->inSet = set;
        this->inPrevValue = (fromValue - 1);
        
        for (int i = 0; i < setSize + 1; i++)
        {
             set[i] = (float)toValue + i * ( ((float)fromValue - (float)toValue) / setSize);
        }
        this->outSet = set;
        this->outPrevValue = (toValue + 1);
    }
    else
    {
        // create a vector of length setSize
        std::vector<float> set(setSize);
        
        // now assign the values to the vector
        for (int i = 0; i < setSize + 1; i++)
            {
                 set[i] = fromValue + (
                                        (toValue - fromValue) *
                                            (exp(fadeCurve*(i*intervalSize))-1) /
                                            (exp(fadeCurve * (setSize * intervalSize))-1)
                                    );
            }
        this->inSet = set;
        this->inPrevValue = (fromValue - 1);
        for (int i = 0; i < setSize + 1; i++)
        {
             set[i] = toValue + (
                                    (fromValue - toValue) *
                                        (exp(fadeCurve*(i*intervalSize))-1) /
                                        (exp(fadeCurve * (setSize * intervalSize))-1)
                                );
        }
        this->outSet = set;
        this->outPrevValue = (toValue + 1);
    }
}

bool StreamDeckMidiButton::FadeSet::UpdateFade()
{
    if (this->fadeActive && this->currentDirection == Direction::IN)
    {
        if (!this->reverseFade)
        {
            if (this->currentIndex == this->setSize)
            {
                this->currentIndex = 0;//reset the currentIndex of the fadeSet
                this->inPrevValue = (this->fromValue - 1);//reset inPrevValue
                this->currentValue = this->toValue;
                this->fadeActive = false;
                this->fadeFinished = true;
                return true;
            }
            else if (this->inPrevValue != floor(this->inSet[this->currentIndex]))
            {
                this->currentValue = floor(this->inSet[this->currentIndex]);
                this->inPrevValue = floor(this->inSet[this->currentIndex]);
                this->currentIndex++;
                return true;
            }
            else
            {
                this->currentIndex++;
                return false;
            }
        }
        else if (this->reverseFade)
        {
            if (this->currentIndex == 0)
            {
                this->inPrevValue = (this->fromValue - 1);//reset inPrevValue
                //this->currentValue = this->fromValue;
                this->fadeActive = false;
                this->fadeFinished = true;
                this->reverseFade = false;
                return false;
            }
            else if (this->inPrevValue != floor(this->inSet[this->currentIndex]))
            {
                this->currentValue = floor(this->inSet[this->currentIndex]);
                this->inPrevValue = floor(this->inSet[this->currentIndex]);
                this->currentIndex--;
                return true;
            }
            else
            {
                this->currentIndex--;
                return false;
            }
        }
    }
    else if (this->fadeActive && this->currentDirection == Direction::OUT)
    {
        if (!this->reverseFade)
        {
            if (this->currentIndex == this->setSize)
            {
                this->currentIndex = 0;//reset the currentIndex of the fadeSet
                this->outPrevValue = (this->toValue + 1);//reset outPrevValue
                //std::cout << this->fromValue << std::endl;
                this->currentValue = this->fromValue;
                this->fadeActive = false;
                this->fadeFinished = true;
                return true;
            }
            else if (this->outPrevValue != ceil(this->outSet[this->currentIndex]))
            {
                //std::cout << ceil(this->outSet[this->currentIndex]) << std::endl;
                this->currentValue = ceil(this->outSet[this->currentIndex]);
                this->outPrevValue = ceil(this->outSet[this->currentIndex]);
                this->currentIndex++;
                return true;
            }
            else
            {
                this->currentIndex++;
                return false;
            }
        }
        else if (this->reverseFade)
        {
            if (this->currentIndex == 0)
            {
                this->outPrevValue = (this->toValue + 1);//reset inPrevValue
                this->currentValue = this->toValue;
                this->fadeActive = false;
                this->fadeFinished = true;
                this->reverseFade = false;
                return true;
            }
            else if (this->outPrevValue != floor(this->outSet[this->currentIndex]))
            {
                this->currentValue = floor(this->outSet[this->currentIndex]);
                this->outPrevValue = floor(this->outSet[this->currentIndex]);
                this->currentIndex--;
                return true;
            }
            else
            {
                this->currentIndex--;
                return false;
            }
        }    }
    return false;
}

void StreamDeckMidiButton::FadeSet::ReverseFade()
{
    if (!this->fadeActive)
    {
        this->currentIndex = this->setSize;
        this->reverseFade = true;
        this->fadeActive = true;
    }
    else if (this->fadeActive)
    {
        this->reverseFade = true;
    }
}

void StreamDeckMidiButton::FadeSet::FadeButtonPressed()
{
    if (this->fadeActive) this->fadeActive = false;
    else this->fadeActive = true;
}


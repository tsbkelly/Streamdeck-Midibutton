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
#define DebugMessage(x) { if (mGlobalSettings->printDebug) mConnectionManager->LogMessage(x); }

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
        //midiOut = new RtMidiOut();
        midiOut = new rtmidi::midi_out();
        if (midiIn != nullptr)
        {
            delete midiIn;
        }
        //midiIn = new RtMidiIn();
        midiIn = new rtmidi::midi_in();
    }
    catch (std::exception e)
    {
        exit(EXIT_FAILURE);
    }
    
    //start the timer with 1ms resolution
    eTimer = new Timer(std::chrono::milliseconds(mGlobalSettings->sampleInterval));
    
    //add an event to the timer and use it to update the fades - replace this with individual timer increments
    auto mTimerUpdate = eTimer->set_interval(std::chrono::milliseconds(mGlobalSettings->sampleInterval), [this]() {this->UpdateTimer();});
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
            Message("bool MidiButton::InitialiseMidi(Direction OUT)");
            try
            {
                if (midiOut != nullptr)
                {
                    Message("bool MidiButton::InitialiseMidi(Direction OUT): midiOut is already initialised - deleting");
                    delete midiOut;
                    Message("bool MidiButton::InitialiseMidi(Direction OUT): creating a new midiOut");
                    midiOut = new rtmidi::midi_out();
                }
#if defined (__APPLE__)
                if (mGlobalSettings->useVirtualPort)
                {
                    DebugMessage("bool MidiButton::InitialiseMidi(Direction OUT): opening virtual OUTPUT port called: " + mGlobalSettings->portName);
                    midiOut->open_virtual_port(mGlobalSettings->portName);
                }
                else
                {
#endif
                    //idiot check - are there any OUTPUT ports?
                    if (midiOut->get_port_count() == 0)
                    {
                        Message("bool MidiButton::InitialiseMidi(Direction OUT): no midi OUT device available");
                        return false;
                    }
                    else
                    {
                        if (mGlobalSettings->selectedOutPortName == midiOut->get_port_name(mGlobalSettings->selectedOutPortIndex))
                        {
                            //the portname and index match - open the port
                            DebugMessage("bool MidiButton::InitialiseMidi(Direction OUT): portName and portIndex match - opening OUTPUT port index: " + std::to_string(mGlobalSettings->selectedOutPortIndex) + " called: " + midiOut->get_port_name(mGlobalSettings->selectedOutPortIndex));
                            midiOut->open_port(mGlobalSettings->selectedOutPortIndex);
                        }
                        else if (mGlobalSettings->selectedOutPortName != midiOut->get_port_name(mGlobalSettings->selectedOutPortIndex))
                        {
                            Message("bool MidiButton::InitialiseMidi(Direction OUT): selectedOutPortName and selectedOutPortIndex DON'T match - setting selectedOutPortIndex to the correct name");
                            for (int i = 0; i < midiOut->get_port_count(); i++)
                            {
                                if (midiOut->get_port_name(i) == mGlobalSettings->selectedOutPortName) mGlobalSettings->selectedOutPortIndex = i;
                            }
                            DebugMessage("bool MidiButton::InitialiseMidi(Direction OUT): opening OUTPUT port index: " + std::to_string(mGlobalSettings->selectedOutPortIndex) + " called: " + midiOut->get_port_name(mGlobalSettings->selectedOutPortIndex));
                            midiOut->open_port(mGlobalSettings->selectedOutPortIndex);
                        }
                        else
                        {
                            Message("bool MidiButton::InitialiseMidi(Direction OUT): selectedOutPortIndex is out of range - something has gone wrong");
                            return false;
                        }
                    }
#if defined (__APPLE__)
                }
#endif
            }
            //catch (RtMidiError &error)
            catch (const rtmidi::midi_exception& error)
            {
                Message("bool MidiButton::InitialiseMidi(Direction OUT): problem with RtMidi - exiting with prejudice");
                Message(error.what());
                exit(EXIT_FAILURE);
            }
        return true;
        }
        case Direction::IN:
        {
            Message("bool MidiButton::InitialiseMidi(Direction IN)");
            midiUpdateMutex.lock();
            try
            {
                if (midiIn != nullptr)
                {
                    Message("bool MidiButton::InitialiseMidi(Direction IN): midiIn is already initialised - deleting");
                    delete midiIn;
                    Message("bool MidiButton::InitialiseMidi(Direction IN): creating a new midiIn");
                    midiIn = new rtmidi::midi_in();
                    midiIn->set_callback([this] (const auto& message) { HandleMidiInput(message); });
                }
                
                // don't ignore sysex, timing or active sensing messages
                midiIn->ignore_types(false, false, false);
#if defined (__APPLE__)
                if (mGlobalSettings->useVirtualPort)
                {
                    DebugMessage("bool MidiButton::InitialiseMidi(Direction IN): opening virtual INPUT port called: " + mGlobalSettings->portName);
                    midiIn->open_virtual_port(mGlobalSettings->portName);
                }
                else
                {
#endif
                    //idiot check - are there any INPUT ports?
                    if (midiIn->get_port_count() == 0)
                    {
                        Message("bool MidiButton::InitialiseMidi(Direction IN): no midi IN device available");
                        midiUpdateMutex.unlock();
                        return false;
                    }
                    else
                    {
                        if (mGlobalSettings->selectedInPortName == midiIn->get_port_name(mGlobalSettings->selectedInPortIndex))
                        {
                            //the portname and index match - open the port
                            DebugMessage("bool MidiButton::InitialiseMidi(Direction IN): portName and portIndex match - opening INPUT port index: " + std::to_string(mGlobalSettings->selectedInPortIndex) + " called: " + midiIn->get_port_name(mGlobalSettings->selectedInPortIndex));
                            midiIn->open_port(mGlobalSettings->selectedInPortIndex);
                        }
                        else if (mGlobalSettings->selectedInPortName != midiIn->get_port_name(mGlobalSettings->selectedInPortIndex))
                        {
                            Message("bool MidiButton::InitialiseMidi(Direction IN): selectedInPortName and selectedInPortIndex DON'T match - setting selectedInPortIndex to the correct name");
                            for (int i = 0; i < midiIn->get_port_count(); i++)
                            {
                                if (midiIn->get_port_name(i) == mGlobalSettings->selectedInPortName) mGlobalSettings->selectedInPortIndex = i;
                            }
                            DebugMessage("bool MidiButton::InitialiseMidi(Direction IN): opening INPUT port index: " + std::to_string(mGlobalSettings->selectedInPortIndex) + " called: " + midiIn->get_port_name(mGlobalSettings->selectedInPortIndex));
                            midiIn->open_port(mGlobalSettings->selectedInPortIndex);
                        }
                        else
                        {
                            Message("bool MidiButton::InitialiseMidi(Direction IN): selectedInPortIndex is out of range - something has gone wrong");
                            midiUpdateMutex.unlock();
                            return false;
                        }
                    }
                }
            }
            //catch (RtMidiError &error)
            catch (const rtmidi::midi_exception& error)
            {
                Message("bool MidiButton::InitialiseMidi(Direction IN): problem with RtMidi - exiting with prejudice");
                Message(error.what());
                exit(EXIT_FAILURE);
            }
            midiUpdateMutex.unlock();
            return true;
#if defined (__APPLE__)
        }
#endif
    }
}

void StreamDeckMidiButton::HandleMidiInput(const rtmidi::message &message)
{
    Message("void StreamDeckMidiButton::HandleMidiInput()");
    /*if(message.is_note_on_or_off())
    {
        mConnectionManager->LogMessage(std::to_string(message.bytes[0]) + " " + std::to_string(message.bytes[1]) + " " + std::to_string(message.bytes[2]));
    }*/

    //deal with midi input
    
    midiUpdateMutex.lock();//lock the mutex to make sure we're not trying to access from more than one place
    
    if (midiIn != nullptr)
    {
        if (mConnectionManager != nullptr)
        {
            std::string debugMessage = "void StreamDeckMidiButton::GetMidiInput()";
            std::string logMessage;
            int nBytes;
            
            auto message = midiIn->get_message();
            nBytes = message.size();
            if (nBytes > 0)
            {
                if (mGlobalSettings->printDebug) debugMessage.append(": received a midi message with " + std::to_string(nBytes) + " bytes, with ");
                for (int i = 0; i < nBytes; i++)
                {
                    debugMessage.append("byte " + std::to_string(i) + " = " + std::to_string((int)message[i]) + ", ");
                }
                debugMessage.append(", stamp = " + std::to_string(message.timestamp));
                Message(debugMessage);

                std::map<std::string, ButtonSettings>::iterator storedButtonSettingsIterator = storedButtonSettings.begin();
                
                while (storedButtonSettingsIterator != storedButtonSettings.end())
                {
                    if (storedButtonSettings[storedButtonSettingsIterator->first].statusByte == (int)message[0])//matching status byte
                    {
                        if (storedButtonSettings[storedButtonSettingsIterator->first].dataByte1 > 0 && storedButtonSettings[storedButtonSettingsIterator->first].dataByte1 == (int)message[1])//matching data byte 1
                        {
                            if (mGlobalSettings->printDebug)
                            {
                                debugMessage.clear();
                                debugMessage = ("void StreamDeckMidiButton::GetMidiInput(): storedButtonSettings status byte for button " + storedButtonSettingsIterator->first + " is " + std::to_string(storedButtonSettings[storedButtonSettingsIterator->first].statusByte) + " which matches incoming status byte of " + std::to_string((int)message[0]) + " and data byte 1 of " + std::to_string((int)message[1]));
                                Message(debugMessage);
                            }

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

/*void StreamDeckMidiButton::UpdateFade()
{
    std::map<std::string, FadeSet>::iterator it = this->storedFadeSettings.begin();
    
    // Iterate over the map using Iterator till end.
    while (it != this->storedFadeSettings.end())
    {
        if (this->storedFadeSettings[it->first].fadeActive)//it->first is the same as inContext
        {
        
        }
    }
}*/

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

void StreamDeckMidiButton::WillAppearForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
    Message("void MidiButton::WillAppearForAction()");
    this->InitialSetup();

    //lock the context
    mVisibleContextsMutex.lock();
    mVisibleContexts.insert(inContext);
    
    //store the button settings
    DebugMessage("void MidiButton::WillAppearForAction(): setting the storedButtonSettings for button: " + inContext);
    StoreButtonSettings(inAction, inContext, inPayload, inDeviceID);
    
    //unlock the context
    mVisibleContextsMutex.unlock();
}

void StreamDeckMidiButton::WillDisappearForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
    Message("void StreamDeckMidiButton::WillDisappearForAction()");
    //remove the context
    mVisibleContextsMutex.lock();
    mVisibleContexts.erase(inContext);
    mVisibleContextsMutex.unlock();
}

void StreamDeckMidiButton::InitialSetup()
{
    std::string debugMessage = "void StreamDeckMidiButton::InitialSetup()";
    try
    {
        std::call_once(initialSetup, [this](){mConnectionManager->GetGlobalSettings();});
    }
    catch (...)
    {
        if (mGlobalSettings->printDebug) debugMessage.append(": haven't called mConnectionManager->GetGlobalSettings() - initial setup complete");//this doesn't work - need to restructure the call to call_once
    }
    Message(debugMessage);
}

void StreamDeckMidiButton::DidReceiveGlobalSettings(const json& inPayload)
{
    if (mGlobalSettings->printDebug)
    {
        Message("void MidiButton::DidReceiveGlobalSettings(): inPayload = " + inPayload.dump());
    }
    else
    {
        Message("void MidiButton::DidReceiveGlobalSettings()");
    }
    //boolean to say settings have changed
    bool midiSettingsChanged = false;
    
    //check to see that the payload has the necessary information, and set the midiSettingsChanged flag if so
    
    //see if the printDebug flag has been set
    if (inPayload["settings"].find("printDebug") != inPayload["settings"].end())
    {
        if (mGlobalSettings->printDebug != inPayload["settings"]["printDebug"])//printDebug has changed
        {
            mGlobalSettings->printDebug = inPayload["settings"]["printDebug"];
            if (mGlobalSettings->printDebug)
            {
                Message("void MidiButton::DidReceiveGlobalSettings(): mGlobalSettings->printDebug is set to " + BoolToString(mGlobalSettings->printDebug) + " - will print all debug messages");
            }
            else if (!mGlobalSettings->printDebug)
            {
                Message("void MidiButton::DidReceiveGlobalSettings(): mGlobalSettings->printDebug is set to " + BoolToString(mGlobalSettings->printDebug) + " - won't print verbose debug messages");
            }
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

    if (!midiSettingsChanged)
    {
        Message("void MidiButton::DidReceiveGlobalSettings(): no settings changed - returning");
        return;//nothing has changed (except possibly the printDebug flag - return
    }
    else if (midiSettingsChanged)
    {
        if (mGlobalSettings->useVirtualPort)
        {
            DebugMessage("void MidiButton::DidReceiveGlobalSettings(): opening the virtual port with portName " + mGlobalSettings->portName);
            if (InitialiseMidi(Direction::IN))
            {
                Message("void MidiButton::DidReceiveGlobalSettings(): MIDI input initialised successfully");
            }
            else
            {
                Message("void MidiButton::DidReceiveGlobalSettings(): something went wrong initialising MIDI input");
                return;
            }
            if (InitialiseMidi(Direction::OUT))
            {
                Message("void MidiButton::DidReceiveGlobalSettings(): MIDI output initialised successfully");
            }
            else
            {
                Message("void MidiButton::DidReceiveGlobalSettings(): something went wrong initialising MIDI output");
                return;
            }
        }
        else if (!mGlobalSettings->useVirtualPort)
        {
            DebugMessage("void MidiButton::DidReceiveGlobalSettings(): opening the physical OUTPUT port with index " + std::to_string(mGlobalSettings->selectedOutPortIndex) + " & port name " + midiOut->get_port_name(mGlobalSettings->selectedOutPortIndex));
            DebugMessage("void MidiButton::DidReceiveGlobalSettings(): opening the physical INPUT port with index " + std::to_string(mGlobalSettings->selectedInPortIndex) + " & port name " + midiOut->get_port_name(mGlobalSettings->selectedInPortIndex));
            
            if (InitialiseMidi(Direction::IN))
            {
                Message("void MidiButton::DidReceiveGlobalSettings(): MIDI input initialised successfully");
            }
            else
            {
                Message("void MidiButton::DidReceiveGlobalSettings(): something went wrong initialising MIDI input");
                return;
            }
            if (InitialiseMidi(Direction::OUT))
            {
                Message("void MidiButton::DidReceiveGlobalSettings(): MIDI output initialised successfully");
            }
            else
            {
                Message("void MidiButton::DidReceiveGlobalSettings(): something went wrong initialising MIDI output");
                return;
            }
        }
    }
}

void StreamDeckMidiButton::DidReceiveSettings(const std::string& inAction, const std::string& inContext, const json& inPayload, const std::string& inDeviceID)
{
    if (mGlobalSettings->printDebug)
    {
        Message("void MidiButton::DidReceiveSettings(): StoreButtonSettings() for button: " + inContext + " with settings: " + inPayload.dump().c_str());
    }
    else
    {
        Message("void MidiButton::DidReceiveSettings()");
    }
    StoreButtonSettings(inAction, inContext, inPayload, inDeviceID);
}

void StreamDeckMidiButton::StoreButtonSettings(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
    if (mGlobalSettings->printDebug)
    {
        Message("void MidiButton::StoreButtonSettings(): received button settings for " + inContext + ", with action " + inAction + ": " + inPayload.dump().c_str());
    }
    else
    {
        Message("void MidiButton::StoreButtonSettings()");
    }
    
    ButtonSettings thisButtonSettings;
    
    //should iterate through the JSON payload here and store everything into a struct
    
    thisButtonSettings.inAction = inAction;
    thisButtonSettings.inContext = inContext;
    
    //STATUS BYTE
    if (inPayload["settings"].find("statusByte") != inPayload["settings"].end())
    {
        thisButtonSettings.statusByte = inPayload["settings"]["statusByte"];
        DebugMessage("void MidiButton::StoreButtonSettings(): Setting statusByte to " + std::to_string(thisButtonSettings.statusByte));
    }
    
    //COMMON
    if (inPayload["settings"].find("dataByte1") != inPayload["settings"].end())
    {
        thisButtonSettings.dataByte1 = inPayload["settings"]["dataByte1"];
        DebugMessage("void MidiButton::StoreButtonSettings(): Setting dataByte1 to " + std::to_string(thisButtonSettings.dataByte1));
    }
    if (inPayload["settings"].find("dataByte2") != inPayload["settings"].end())
    {
        thisButtonSettings.dataByte2 = inPayload["settings"]["dataByte2"];
        DebugMessage("void MidiButton::StoreButtonSettings(): Setting dataByte2 to " + std::to_string(thisButtonSettings.dataByte2));
    }
    if (inPayload["settings"].find("dataByte2Alt") != inPayload["settings"].end())
    {
        thisButtonSettings.dataByte2Alt = inPayload["settings"]["dataByte2Alt"];
        DebugMessage("void MidiButton::StoreButtonSettings(): Setting dataByte2Alt to " + std::to_string(thisButtonSettings.dataByte2Alt));
    }
    if (inPayload["settings"].find("dataByte5") != inPayload["settings"].end())
    {
        thisButtonSettings.dataByte5 = inPayload["settings"]["dataByte5"];
        DebugMessage("void MidiButton::StoreButtonSettings(): Setting dataByte5 to " + std::to_string(thisButtonSettings.dataByte5));
    }
    
    //NOTE OFF mode
    if (inPayload["settings"].find("noteOffMode") != inPayload["settings"].end())
    {
        thisButtonSettings.noteOffMode = inPayload["settings"]["noteOffMode"];
        DebugMessage("void MidiButton::StoreButtonSettings(): Setting noteOffMode to " + std::to_string(thisButtonSettings.noteOffMode));
    }

    //VARIOUS
    if (inPayload["settings"].find("toggleFade") != inPayload["settings"].end())
    {
        thisButtonSettings.toggleFade = inPayload["settings"]["toggleFade"];
        DebugMessage("void MidiButton::StoreButtonSettings(): Setting toggleFade to " + BoolToString(thisButtonSettings.toggleFade));
    }
    if (inPayload["settings"].find("fadeTime") != inPayload["settings"].end())
    {
        thisButtonSettings.fadeTime = inPayload["settings"]["fadeTime"];
        DebugMessage("void MidiButton::StoreButtonSettings(): Setting fadeTime to " + std::to_string(thisButtonSettings.fadeTime));
    }
    if (inPayload["settings"].find("fadeCurve") != inPayload["settings"].end())
    {
        thisButtonSettings.fadeCurve = inPayload["settings"]["fadeCurve"];
        DebugMessage("void MidiButton::StoreButtonSettings(): Setting fadeCurve to " + std::to_string(thisButtonSettings.fadeCurve));
    }
    if (inPayload["settings"].find("ccMode") != inPayload["settings"].end())
    {
        thisButtonSettings.ccMode = inPayload["settings"]["ccMode"];
        DebugMessage("void MidiButton::StoreButtonSettings(): Setting ccMode to " + std::to_string(thisButtonSettings.ccMode));
        //if ccMode is 2 or 3 set the toggleFade flag
        if (thisButtonSettings.ccMode == 2 || thisButtonSettings.ccMode == 3)
        {
            thisButtonSettings.toggleFade = true;
            DebugMessage("void MidiButton::StoreButtonSettings(): ccMode is 2 or 3 - setting toggleFade to " + BoolToString(thisButtonSettings.toggleFade));
        }
        else
        {
            thisButtonSettings.toggleFade = false;
            DebugMessage("void MidiButton::StoreButtonSettings(): ccMode is 0 or 1 - setting toggleFade to " + BoolToString(thisButtonSettings.toggleFade));
        }
    }
    
    //store everything into the map
    if (storedButtonSettings.insert(std::make_pair(inContext, thisButtonSettings)).second == false)
    {
        //key already exists, so replace it
        DebugMessage("void MidiButton::StoreButtonSettings(): Key already exists - replacing");
        storedButtonSettings[inContext] = thisButtonSettings;
    }
    
    // NEED TO CHANGE THIS DEBUG SECTION
    if (mGlobalSettings->printDebug)//check it's been stored correctly
    {
        //iterate through settings and log messages - should be a way to do this in a loop
        if (inAction == SEND_NOTE_ON || SEND_NOTE_ON_TOGGLE)
        {
            Message("void MidiButton::StoreButtonSettings(): midiChannel for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].statusByte - NOTE_ON));
            Message("void MidiButton::StoreButtonSettings(): dataByte1 for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].dataByte1));
            Message("void MidiButton::StoreButtonSettings(): dataByte2 for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].dataByte2));
            if (inAction == SEND_NOTE_ON) Message("void MidiButton::StoreButtonSettings(): noteOffMode for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].noteOffMode));
        }
        else if (inAction == SEND_CC || SEND_CC_TOGGLE)
        {
            Message("void MidiButton::StoreButtonSettings(): midiChannel for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].statusByte - CC));
            Message("void MidiButton::StoreButtonSettings(): dataByte1 for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].dataByte1));
            Message("void MidiButton::StoreButtonSettings(): dataByte2 for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].dataByte2));
            if (inAction == SEND_CC_TOGGLE) Message("void MidiButton::StoreButtonSettings(): dataByte2Alt for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].dataByte2Alt));
            Message("void MidiButton::StoreButtonSettings(): toggleFade for " + inContext + " is set to: " + BoolToString(storedButtonSettings[inContext].toggleFade));
            if (storedButtonSettings[inContext].toggleFade)
            {
                Message("void MidiButton::StoreButtonSettings(): fadeTime for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].fadeTime));
                Message("void MidiButton::StoreButtonSettings(): fadeCurve for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].fadeCurve));
            }
        }
        else if (inAction == SEND_PROGRAM_CHANGE)
        {
            Message("void MidiButton::StoreButtonSettings(): midiChannel for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].statusByte - PC));
            Message("void MidiButton::StoreButtonSettings(): dataByte1 for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].dataByte1));
        }
        else if (inAction == SEND_MMC)
        {
            Message("void MidiButton::StoreButtonSettings(): dataByte5 for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].dataByte5));
        }
    }

    //if it's an MMC button set the icon
    if (inAction == SEND_MMC)
    {
        DebugMessage("void MidiButton::StoreButtonSettings(): inAction == SEND_MMC -> SetActionIcon()");
        SetActionIcon(inAction, inContext, inPayload, inDeviceID);
    }
    
    //if it's a CC Toggle button generate the fadeSet, if required
    if (storedButtonSettings[inContext].toggleFade)//create the fadeSet for the button
    {
        if (storedButtonSettings[inContext].fadeTime == 0)
        {
            DebugMessage("void MidiButton::StoreButtonSettings(): fadeTime of 0! - divide by zero error, so ignoring by switching toggleFade off");
            storedButtonSettings[inContext].toggleFade = false; //to avoid divide by zero problem
        }
        else
        {
            DebugMessage("void MidiButton::StoreButtonSettings(): Generating the fade lookup table");
            FadeSet thisButtonFadeSet(storedButtonSettings[inContext].dataByte2, storedButtonSettings[inContext].dataByte2Alt, storedButtonSettings[inContext].fadeTime, storedButtonSettings[inContext].fadeCurve, mGlobalSettings->sampleInterval);
            
            if (mGlobalSettings->printDebug)
            {
                int previousValue = 0;
                int previousI = 0;
                std::string outString;
                auto timenow = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                outString.append(ctime(&timenow));
                outString.append("\n");
                outString.append("dumping FadeSet for button " + inContext + " with parameters dataByte2 " + std::to_string(storedButtonSettings[inContext].dataByte2) + ", dataByte2Alt " + std::to_string(storedButtonSettings[inContext].dataByte2Alt) + ", fadeTime " + std::to_string(storedButtonSettings[inContext].fadeTime) + ", fadeCurve " + std::to_string(storedButtonSettings[inContext].fadeCurve) + " with sampleInterval " +  std::to_string(mGlobalSettings->sampleInterval) + "\n");
                outString.append("Index\tValue\tDelta\n");

                for(std::size_t i = 0; i < storedFadeSettings[inContext].inSet.size(); i++)
                {
                    if (floor(storedFadeSettings[inContext].inSet[i]) != previousValue)
                    {
                        //Message(std::to_string(i) + "," + std::to_string(floor(storedFadeSettings[inContext].inSet[i])) + "," + std::to_string((i - previousI)));
                        //outString.append(std::to_string(i) + "," + std::to_string(floor(storedFadeSettings[inContext].inSet[i])) + "," + std::to_string((i - previousI)) + "\n");
                        outString.append(std::to_string(i) + "\t" + std::to_string(floor(storedFadeSettings[inContext].inSet[i])) + "\t" + std::to_string((i - previousI)) + "\n");

                        previousValue = floor(storedFadeSettings[inContext].inSet[i]);
                        previousI = i;
                    }
                }
                outString.append("\n");
                
                auto t = std::time(nullptr);
                auto tm = *std::localtime(&t);

                std::ostringstream oss;
                //oss << std::put_time(&tm, "%d-%m-%Y %H-%M-%S");
                oss << std::put_time(&tm, "[%d-%m-%Y]");
                auto fileName = "debug " + oss.str() + ".txt";

                if (WriteFile(fileName.c_str(), outString))
                {
                    Message("void MidiButton::StoreButtonSettings(): output file written successfully");
                }
                else
                {
                    Message("void MidiButton::StoreButtonSettings(): error writing output file");
                }
            }

            if (storedFadeSettings.insert(std::make_pair(inContext, thisButtonFadeSet)).second == false)
            {
                //key already exists, so replace it
                DebugMessage("void MidiButton::StoreButtonSettings(): FadeSet already exists - replacing");
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
    Message("void MidiButton::KeyDownForAction()");
    DebugMessage("void MidiButton::KeyDownForAction(): dumping the JSON payload: " + inPayload.dump());
    //Message(inPayload.dump().c_str());//REALLY USEFUL for debugging
    
    if (storedButtonSettings.find(inContext) == storedButtonSettings.end())//something's gone wrong - no settings stored, so the PI probably hasn't been opened
    {
        DebugMessage("void MidiButton::KeyDownForAction(): No storedButtonSettings - something went wrong");
        StoreButtonSettings(inAction, inContext, inPayload, inDeviceID);
    }

    try
    {
        if (inAction == SEND_NOTE_ON)
        {
            DebugMessage("void MidiButton::KeyDownForAction(): inAction " + inAction + " for inContext " + inContext);
            midiMessageMutex.lock();
            midiMessage.clear();
            midiMessage.push_back(storedButtonSettings[inContext].statusByte);
            midiMessage.push_back(storedButtonSettings[inContext].dataByte1);
            midiMessage.push_back(storedButtonSettings[inContext].dataByte2);
            SendMidiMessage(midiMessage);
            midiMessageMutex.unlock();
            
            switch (storedButtonSettings[inContext].noteOffMode)
            {
                case 0://no note off message
                {
                    DebugMessage("void MidiButton::SendNoteOn(noteOffMode = 0): no note off message");
                    break;
                }
                case PUSH_NOTE_OFF://1: {//send note off message immediately
                {
                    DebugMessage("void MidiButton::SendNoteOn(noteOffMode = 1): send note off immediately");
                    
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
                case RELEASE_NOTE_OFF://2: {//send note off on KeyUp
                {
                    DebugMessage("void MidiButton::SendNoteOn(noteOffMode = 2): send note off on KeyUp");
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
                Message("void MidiButton::KeyDownForAction(): inAction " + inAction + " for inContext " + inContext);
                if (inPayload.contains("state")) Message("void MidiButton::KeyDownForAction(): we have a state of " + std::to_string(inPayload["state"].get<int>()));
                if (inPayload.contains("userDesiredState")) Message("void MidiButton::KeyDownForAction(): we have a userDesiredState of " + std::to_string(inPayload["userDesiredState"].get<int>()));
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
            else Message("void MidiButton::KeyDownForAction(): something went wrong - should have a state, and we don't have");
        }
        else if (inAction == SEND_CC)
        {
            DebugMessage("void MidiButton::KeyDownForAction(): inAction " + inAction + " for inContext " + inContext);
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
                Message("void MidiButton::KeyDownForAction(): inAction " + inAction + " for inContext " + inContext);
                if (inPayload.contains("state")) Message("void MidiButton::KeyDownForAction(): we have a state of " + std::to_string(inPayload["state"].get<int>()));
                if (inPayload.contains("userDesiredState")) Message("void MidiButton::KeyDownForAction(): we have a userDesiredState of " + std::to_string(inPayload["userDesiredState"].get<int>()));
            }
            if (inPayload.contains("state"))
            {
                if (inPayload.contains("userDesiredState"))
                {
                    if (inPayload["userDesiredState"].get<int>() == 0)
                    {
                        if (!storedButtonSettings[inContext].toggleFade)
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
            else Message("void MidiButton::KeyDownForAction(): Something's gone wrong - should have a state, and we don't have");
        }
        else if (inAction == SEND_PROGRAM_CHANGE)
        {
            DebugMessage("void MidiButton::KeyDownForAction(): inAction " + inAction + " for inContext " + inContext);
            midiMessageMutex.lock();
            midiMessage.clear();
            midiMessage.push_back(storedButtonSettings[inContext].statusByte);
            midiMessage.push_back(storedButtonSettings[inContext].dataByte1);
            SendMidiMessage(midiMessage);
            midiMessageMutex.unlock();
        }
        else if (inAction == SEND_MMC)
        {
            DebugMessage("void MidiButton::KeyDownForAction(): inAction " + inAction + " for inContext " + inContext);
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
            Message("void MidiButton::KeyDownForAction(): Something's gone wrong - no inAction sent, which shouldn't be possible");
        }
    }
    catch (std::exception e)
    {
        Message("void MidiButton::KeyDownForAction(): Something's gone wrong - an unknown error has occurred as below");
        Message(e.what());
    }
}

void StreamDeckMidiButton::KeyUpForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
    Message("void MidiButton::KeyUpForAction()");
    try
    {
        if (inAction == SEND_NOTE_ON)
        {
            if (storedButtonSettings[inContext].noteOffMode == 2)//(noteOffMode == 2)
            {
                DebugMessage("void MidiButton::KeyUpForAction(): send note off");
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
                    DebugMessage("void MidiButton::KeyUpForAction(): send secondary CC value");
                    midiMessageMutex.lock();
                    midiMessage.clear();
                    midiMessage.push_back(storedButtonSettings[inContext].statusByte);
                    midiMessage.push_back(storedButtonSettings[inContext].dataByte1);
                    midiMessage.push_back(storedButtonSettings[inContext].dataByte2Alt);
                    SendMidiMessage(midiMessage);
                    midiMessageMutex.unlock();
                    break;
                case 2: case 3://fade OUT after fading IN
                    DebugMessage("void MidiButton::KeyUpForAction(): ReverseFade()");
                    if (storedFadeSettings[inContext].fadeActive) storedFadeSettings[inContext].ReverseFade();
                    else if (!storedFadeSettings[inContext].fadeActive) storedFadeSettings[inContext].fadeActive = true;
                    break;
            }
        }
        else return;
    }
    catch (std::exception e)
    {
        Message("void MidiButton::KeyUpForAction(): something went wrong - error message follows");
        Message(e.what());
    }
}

void StreamDeckMidiButton::DeviceDidConnect(const std::string& inDeviceID, const json &inDeviceInfo)
{
    Message("void MidiButton::DeviceDidConnect()");
}

void StreamDeckMidiButton::DeviceDidDisconnect(const std::string& inDeviceID)
{
    Message("void MidiButton::DeviceDidDisconnect()");
}

void StreamDeckMidiButton::ChangeButtonState(const std::string& inContext)
{
    if (mGlobalSettings->printDebug)
    {
        Message("void MidiButton::ChangeButtonState(): inContext" + inContext + ", and required state " + std::to_string(storedButtonSettings[inContext].state));
    }
    else
    {
        Message("void MidiButton::ChangeButtonState()");
    }
    mConnectionManager->SetState(storedButtonSettings[inContext].state, inContext);
}

void StreamDeckMidiButton::SendToPlugin(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
    if (mGlobalSettings->printDebug)
    {
        Message("void MidiButton::SendToPlugin(): " + inPayload.dump());
    }
    else
    {
        Message("void MidiButton::SendToPlugin()");
    }
    
    if (inAction == SEND_MMC)
    {
        DebugMessage("void MidiButton::SendToPlugin(): new MMC action - set the action icon");
        SetActionIcon(inAction, inContext, inPayload, inDeviceID);
    }
    
    const auto event = EPLJSONUtils::GetStringByName(inPayload, "event");
    if (event == "getMidiPorts")
    {
        std::map <std::string, std::string> midiPortList = GetMidiPortList(Direction::OUT);
        DebugMessage("void MidiButton::SendToPlugin(): get list of MIDI OUT ports - " + json({{"event", "midiOutPorts"},{"midiOutPortList", midiPortList}}).dump());
        mConnectionManager->SendToPropertyInspector(inAction, inContext,json({{"event", "midiOutPorts"},{"midiOutPortList", midiPortList}}));
        
        DebugMessage("void MidiButton::SendToPlugin(): selected MIDI OUT port - " + json({{"event", "midiOutPortSelected"},{"midiOutPortSelected", mGlobalSettings->selectedOutPortIndex}}).dump());
        mConnectionManager->SendToPropertyInspector(inAction, inContext,json({{"event", "midiOutPortSelected"},{"midiOutPortSelected", mGlobalSettings->selectedOutPortIndex}}));
        
        midiPortList = GetMidiPortList(Direction::IN);
        DebugMessage("void MidiButton::SendToPlugin(): get list of MIDI IN ports - " + json({{"event", "midiInPorts"},{"midiInPortList", midiPortList}}).dump());
        mConnectionManager->SendToPropertyInspector(inAction, inContext,json({{"event", "midiInPorts"},{"midiInPortList", midiPortList}}));
        
        DebugMessage(json({{"event", "midiInPortSelected"},{"midiInPortSelected", mGlobalSettings->selectedInPortIndex}}).dump().c_str());
        mConnectionManager->SendToPropertyInspector(inAction, inContext,json({{"event", "midiInPortSelected"},{"midiInPortSelected", mGlobalSettings->selectedInPortIndex}}));
        return;
    }
    //else something's gone wrong
    else Message("void MidiButton::SendToPlugin(): something went wrong - not expecting this message to be sent. Dumping payload: " + inPayload.dump());
}

void StreamDeckMidiButton::SendMidiMessage(const std::vector<unsigned char>& midiMessage)
{
    std::string debugMessage = "void MidiButton::SendMidiMessage()";
    if (mGlobalSettings->printDebug)
    {
        int nBytes = midiMessage.size();
        debugMessage.append(": sending MIDI message with ");
        for (int i=0; i<nBytes; i++)
        {
            debugMessage.append("byte " + std::to_string(i+1) + ": " + std::to_string((int)midiMessage[i]) + " ");
        }
    }
    Message(debugMessage);
    //midiOut->sendMessage(&midiMessage);
    midiOut->send_message(midiMessage);
}

void StreamDeckMidiButton::SetActionIcon(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
    Message("void MidiButton::SetActionIcon()");
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
        Message("void MidiButton::SetActionIcon(): something went wrong setting the action icon - the file is probably missing/unreadable");
    }
}

std::map <std::string, std::string> StreamDeckMidiButton::GetMidiPortList(Direction direction)
{
    switch (direction)
    {
        case Direction::OUT:
        {
            Message("std::map <std::string, std::string> MidiButton::GetMidiPortList(Direction OUT)");
            std::map <std::string, std::string> midiPortList;
            std::string portName;
            //unsigned int nPorts = midiOut->getPortCount();
            unsigned int nPorts = midiOut->get_port_count();
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
                    //portName = midiOut->getPortName(i);
                    portName = midiOut->get_port_name(i);
                    midiPortList.insert(std::make_pair(portName, std::to_string(i)));
                    count++;
                }
            }
            
            if (mGlobalSettings->printDebug)
            {
                std::map<std::string, std::string>::iterator it = midiPortList.begin();
                while(it != midiPortList.end())
                {
                    Message("std::map <std::string, std::string> MidiButton::GetMidiPortList(Direction OUT): " + it->first + " has port index: " + it->second);
                    it++;
                }
            }
            return midiPortList;
        }
        case Direction::IN:
        {
            Message("std::map <std::string, std::string> MidiButton::GetMidiPortList(Direction IN)");
            std::map <std::string, std::string> midiPortList;
            std::string portName;
            //unsigned int nPorts = midiIn->getPortCount();
            unsigned int nPorts = midiIn->get_port_count();
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
                    //portName = midiIn->getPortName(i);
                    portName = midiIn->get_port_name(i);
                    midiPortList.insert(std::make_pair(portName, std::to_string(i)));
                    count++;
                }
            }
            
            if (mGlobalSettings->printDebug)
            {
                std::map<std::string, std::string>::iterator it = midiPortList.begin();
                while(it != midiPortList.end())
                {
                    Message("std::map <std::string, std::string> MidiButton::GetMidiPortList(Direction IN): " + it->first + " has port index: " + it->second);
                    it++;
                }
            }
            return midiPortList;
        }
    }
}

std::string StreamDeckMidiButton::GetPngAsString(const char* filename)
{
    Message("std::string MidiButton::GetPngAsString()");
    //try reading the file
    std::vector<unsigned char> imageFile = ReadFile(filename);
    if (imageFile.empty())
    {
        Message("std::string MidiButton::GetPngAsString(): empty image file");
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
    Message("std::vector<unsigned char> MidiButton::ReadFile()");
    // open the file:
    std::ifstream file(filename, std::ios::binary);
    std::vector<unsigned char> vec;
    
    if (!filename)
    {
        Message("std::vector<unsigned char> MidiButton::ReadFile(): couldn't open file " + std::string(filename));
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

bool StreamDeckMidiButton::WriteFile(const char* filename, std::string string)
{
    Message("bool MidiButton::WriteFile()");
    // open the file:
    std::ofstream outFile;
    outFile.open (filename, std::ofstream::out | std::ofstream::app);

    if (!filename)
    {
        Message("bool MidiButton::WriteFile(): couldn't open file " + std::string(filename));
        //return vec;
        return false;
    }
    if (outFile.is_open())
    {
        outFile << string;
        outFile.close();
        return true;
    }
    return false;
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
    this->fadeActive = !this->fadeActive;
    //if (this->fadeActive) this->fadeActive = false;
    //else this->fadeActive = true;
}

//==============================================================================
/**
@file       StreamDeckMidiButton.cpp

@brief      MIDI plugin

@copyright  (c) 2020, Clarion Music Ltd
			This source code is licensed under the MIT-style license found in the LICENSE file.
            Parts of this plugin (c) 2019 Fred Emmott, (c) 2018 Corsair Memory, Inc

**/
//==============================================================================

#include "StreamDeckMidiButton.h"
#include "Common/ESDConnectionManager.h"
#include "Common/EPLJSONUtils.h"

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

CallBackTimer::CallBackTimer() :_execute(false) { }

CallBackTimer::~CallBackTimer()
{
    if(_execute.load(std::memory_order_acquire))
    {
        stop();
    };
}

void CallBackTimer::stop()
{
    _execute.store(false, std::memory_order_release);
    if(_thd.joinable())
        _thd.join();
}

void CallBackTimer::start(int interval, std::function<void(void)> func)
{
    if(_execute.load(std::memory_order_acquire))
    {
        stop();
    };
    _execute.store(true, std::memory_order_release);
    _thd = std::thread([this, interval, func]()
    {
        while (_execute.load(std::memory_order_acquire))
        {
            func();
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }
    });
}

bool CallBackTimer::is_running() const noexcept
{
    return (_execute.load(std::memory_order_acquire) && _thd.joinable());
}

StreamDeckMidiButton::StreamDeckMidiButton()
{
    mTimer = new CallBackTimer();
    mTimer->start(5, [this]()
    {
        this->UpdateTimer();
    });
}

StreamDeckMidiButton::~StreamDeckMidiButton()
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
    if(mTimer != nullptr)
    {
        mTimer->stop();
        delete mTimer;
        mTimer = nullptr;
    }

}

bool StreamDeckMidiButton::InitialiseMidi(Direction direction)
{
    switch (direction)
    {
        case Direction::OUTPUT:
        {
            try
            {
                if (midiOut != nullptr)
                {
                    mConnectionManager->LogMessage("midiOut is already initialised - deleting");
                    delete midiOut;
                }
                midiOut = new RtMidiOut();
#if defined (__APPLE__)
                if (mGlobalSettings.useVirtualPort)
                {
                    if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("opening virtual OUTPUT port called: " + mGlobalSettings.portName);
                    midiOut->openVirtualPort(mGlobalSettings.portName);
                }
                else
                {
#endif
                    // open the selected OUTPUT port
                    if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("opening OUTPUT port index: " + std::to_string(mGlobalSettings.selectedOutPortIndex) + " called: " + midiOut->getPortName(mGlobalSettings.selectedOutPortIndex));
                    midiOut->openPort(mGlobalSettings.selectedOutPortIndex);
#if defined (__APPLE__)
                }
#endif
            }
            catch (RtMidiError &error)
            {
                if (mGlobalSettings.printDebug) mConnectionManager->LogMessage(error.getMessage());
                exit(EXIT_FAILURE);
            }
            return true;
        }
        case Direction::INPUT:
        {
            try
            {
                if (midiIn != nullptr)
                {
                    mConnectionManager->LogMessage("midiIn is already initialised - deleting");
                    delete midiIn;
                }
                midiIn = new RtMidiIn();
                
                // don't ignore sysex, timing or active sensing messages
                midiIn->ignoreTypes(false, false, false);
#if defined (__APPLE__)
                if (mGlobalSettings.useVirtualPort)
                {
                    if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("opening virtual INPUT port called: " + mGlobalSettings.portName);
                    midiIn->openVirtualPort(mGlobalSettings.portName);
                }
                else
                {
#endif
                    // open the selected INPUT port
                    if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("opening INPUT port index: " + std::to_string(mGlobalSettings.selectedInPortIndex) + " called: " + midiOut->getPortName(mGlobalSettings.selectedInPortIndex));
                    midiIn->openPort(mGlobalSettings.selectedInPortIndex);
                }
            }
            catch (RtMidiError &error)
            {
                if (mGlobalSettings.printDebug) mConnectionManager->LogMessage(error.getMessage());
                exit(EXIT_FAILURE);
            }
            return true;
#if defined (__APPLE__)
        }
#endif
    }
}

void StreamDeckMidiButton::UpdateTimer()
{
    //
    // Warning: UpdateTimer() is running in the timer thread
    //
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
                for (int i = 0; i < nBytes; i++)
                {
                    //mConnectionManager->LogMessage("Byte " + std::to_string(i) + " = " + std::to_string((int)message[i]) + ", stamp = " + std::to_string(stamp));
                    if ((int)message[i] == 247) mConnectionManager->LogMessage("Byte " + std::to_string(i) + " = " + intToHex((int)message[i]) + " -> EOM");
                    else mConnectionManager->LogMessage("Byte " + std::to_string(i) + " = " + intToHex((int)message[i]));
                }
            }
        }
    }
}

void StreamDeckMidiButton::StoreButtonSettings(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
    if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("Setting button settings for " + inContext + ", with action " + inAction);
    buttonSettings thisButtonSettings;
    
    //should iterate through the JSON payload here and store everything into a struct
    if (inPayload["settings"].find("midiChannel") != inPayload["settings"].end())
    {
        thisButtonSettings.midiChannel = inPayload["settings"]["midiChannel"];
        if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("Setting midiChannel to " + std::to_string(thisButtonSettings.midiChannel));
    }
    if (inPayload["settings"].find("midiNote") != inPayload["settings"].end())
    {
        thisButtonSettings.midiNote = inPayload["settings"]["midiNote"];
        if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("Setting midiNote to " + std::to_string(thisButtonSettings.midiNote));
    }
    if (inPayload["settings"].find("midiVelocity") != inPayload["settings"].end())
    {
        thisButtonSettings.midiVelocity = inPayload["settings"]["midiVelocity"];
        if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("Setting midiVelocity to " + std::to_string(thisButtonSettings.midiVelocity));
    }
    if (inPayload["settings"].find("noteOffParams") != inPayload["settings"].end())
    {
        std::string mNO = inPayload["settings"]["noteOffParams"];
        std::stringstream MNO(mNO);
        int noteOffParams = 0;
        MNO >> noteOffParams;
        thisButtonSettings.noteOffParams = noteOffParams;
        if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("Setting noteOffParams to " + std::to_string(thisButtonSettings.noteOffParams));
    }
    if (inPayload["settings"].find("midiProgramChange") != inPayload["settings"].end())
    {
        thisButtonSettings.midiProgramChange = inPayload["settings"]["midiProgramChange"];
        if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("Setting midiProgramChange to " + std::to_string(thisButtonSettings.midiProgramChange));
    }
    if (inPayload["settings"].find("midiCC") != inPayload["settings"].end())
    {
        thisButtonSettings.midiCC = inPayload["settings"]["midiCC"];
        if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("Setting midiCC to " + std::to_string(thisButtonSettings.midiCC));
    }
    if (inPayload["settings"].find("midiValue") != inPayload["settings"].end())
    {
        thisButtonSettings.midiValue = inPayload["settings"]["midiValue"];
        if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("Setting midiValue to " + std::to_string(thisButtonSettings.midiValue));
    }
    if (inPayload["settings"].find("midiValueSec") != inPayload["settings"].end())
    {
        thisButtonSettings.midiValueSec = inPayload["settings"]["midiValueSec"];
        if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("Setting midiValueSec to " + std::to_string(thisButtonSettings.midiValueSec));
    }
    if (inPayload["settings"].find("midiMMC") != inPayload["settings"].end())
    {
        thisButtonSettings.midiMMC = inPayload["settings"]["midiMMC"];
        if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("Setting midiMMC to " + std::to_string(thisButtonSettings.midiMMC));
    }
    if (inPayload["settings"].find("toggleCC") != inPayload["settings"].end())
    {
        thisButtonSettings.toggleCC = inPayload["settings"]["toggleCC"];
        if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("Setting toggleCC to " + std::to_string(thisButtonSettings.toggleCC));
    }
    
    //store everything into the map
    if (storedButtonSettings.insert(std::make_pair(inContext, thisButtonSettings)).second == false)
    {
        //key already exists, so replace it
        if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("Key already exists - replacing");
        storedButtonSettings[inContext] = thisButtonSettings;
    }
    
    //if it's an MMC button set the icon
    if (inAction == SEND_MMC) SetActionIcon(inAction, inContext, inPayload, inDeviceID);
    
    if (mGlobalSettings.printDebug)//check it's been stored correctly
    {
        //iterate through settings and log messages - should be a way to do this in a loop
        mConnectionManager->LogMessage("midiChannel for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].midiChannel));
        mConnectionManager->LogMessage("midiNote for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].midiNote));
        mConnectionManager->LogMessage("midiVelocity for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].midiVelocity));
        mConnectionManager->LogMessage("noteOffParams for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].noteOffParams));
        mConnectionManager->LogMessage("midiProgramChange for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].midiProgramChange));
        mConnectionManager->LogMessage("midiCC for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].midiCC));
        mConnectionManager->LogMessage("midiValue for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].midiValue));
        mConnectionManager->LogMessage("midiValueSec for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].midiValueSec));
        mConnectionManager->LogMessage("midiMMC for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].midiMMC));
    }
}

void StreamDeckMidiButton::KeyDownForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
    
    if (mGlobalSettings.printDebug)
    {
        mConnectionManager->LogMessage("StreamDeckMidiButton::KeyDownForAction() - dumping the JSON payload:");
        mConnectionManager->LogMessage(inPayload.dump().c_str());//REALLY USEFUL for debugging
    }
    
    if (storedButtonSettings.find(inContext) == storedButtonSettings.end())//something's gone wrong - no settings stored, so the PI probably hasn't been opened
    {
        if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("No storedButtonSettings - something's gone wrong");
        StoreButtonSettings(inAction, inContext, inPayload, inDeviceID);
    }

    try
    {
        if (inAction == SEND_NOTE_ON)
        {
            if (mGlobalSettings.printDebug)
            {
                mConnectionManager->LogMessage("inAction: " + inAction);
                mConnectionManager->LogMessage("inContext: " + inContext);
            }
            
            if (storedButtonSettings[inContext].noteOffParams != 3) SendNoteOn(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiNote, storedButtonSettings[inContext].midiVelocity, storedButtonSettings[inContext].noteOffParams);
            
            else if (storedButtonSettings[inContext].noteOffParams == 3)
            {
                if (storedButtonSettings[inContext].toggleNoteOnOff)
                {
                    if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("toggleNoteOnOff is set to " + BoolToString(storedButtonSettings[inContext].toggleNoteOnOff));
                    SendNoteOn(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiNote, storedButtonSettings[inContext].midiVelocity, storedButtonSettings[inContext].noteOffParams);
                    storedButtonSettings[inContext].toggleNoteOnOff = !storedButtonSettings[inContext].toggleNoteOnOff;
                }
                else if (!storedButtonSettings[inContext].toggleNoteOnOff)
                {
                    if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("toggleNoteOnOff is set to " + BoolToString(storedButtonSettings[inContext].toggleNoteOnOff));
                    SendNoteOff(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiNote, storedButtonSettings[inContext].midiVelocity);
                    storedButtonSettings[inContext].toggleNoteOnOff = !storedButtonSettings[inContext].toggleNoteOnOff;
                }
            }
        }
        else if (inAction == SEND_CC)
        {
            if (mGlobalSettings.printDebug)
            {
                mConnectionManager->LogMessage("inAction: " + inAction);
                mConnectionManager->LogMessage("inContext: " + inContext);
            }
            
            if (storedButtonSettings[inContext].toggleCC)
            {
                if (storedButtonSettings[inContext].toggleCCState)
                {
                    if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("toggleCCState is set to " + BoolToString(storedButtonSettings[inContext].toggleCCState));
                    SendCC(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiCC, storedButtonSettings[inContext].midiValue);
                    storedButtonSettings[inContext].toggleCCState = !storedButtonSettings[inContext].toggleCCState;
                }
                else if (!storedButtonSettings[inContext].toggleCCState)
                {
                    if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("toggleCCState is set to " + BoolToString(storedButtonSettings[inContext].toggleCCState));
                    SendCC(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiCC, storedButtonSettings[inContext].midiValueSec);
                    storedButtonSettings[inContext].toggleCCState = !storedButtonSettings[inContext].toggleCCState;
                }
            }

            else if (!storedButtonSettings[inContext].toggleCC)
            {
                SendCC(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiCC, storedButtonSettings[inContext].midiValue);
            }
        }
        else if (inAction == SEND_CC_TOGGLE)
        {
            if (mGlobalSettings.printDebug)
            {
                mConnectionManager->LogMessage("inAction: " + inAction);
                mConnectionManager->LogMessage("inContext: " + inContext);
                if (inPayload.contains("state")) mConnectionManager->LogMessage("we have a state of " + std::to_string(inPayload["state"].get<int>()));
                if (inPayload.contains("userDefinedState")) mConnectionManager->LogMessage("we have a userDesiredState of " + std::to_string(inPayload["userDesiredState"].get<int>()));
            }
            if (inPayload.contains("state"))
            {
                if (inPayload.contains("userDesiredState"))
                {
                    if (inPayload["userDesiredState"].get<int>() == 0) SendCC(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiCC, storedButtonSettings[inContext].midiValue);
                    else if (inPayload["userDesiredState"].get<int>() == 1) SendCC(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiCC, storedButtonSettings[inContext].midiValueSec);
                }
                else
                {
                    if (inPayload["state"].get<int>() == 0) SendCC(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiCC, storedButtonSettings[inContext].midiValue);
                    else if (inPayload["state"].get<int>() == 1) SendCC(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiCC, storedButtonSettings[inContext].midiValueSec);
                }
            }
            else mConnectionManager->LogMessage("Something's gone wrong - should have a state, and we don't have");
        }
        else if (inAction == SEND_PROGRAM_CHANGE)
        {
            if (mGlobalSettings.printDebug) mConnectionManager->LogMessage(inAction);
            SendProgramChange(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiProgramChange);
        }
        else if (inAction == SEND_MMC)
        {
            if (mGlobalSettings.printDebug) mConnectionManager->LogMessage(inAction);
            SendMMC(storedButtonSettings[inContext].midiMMC);
        }
        else
        {
            //something has gone wrong
            if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("Something's gone wrong...");
        }
    }
    catch (std::exception e)
    {
        mConnectionManager->LogMessage(e.what());
    }
}

void StreamDeckMidiButton::KeyUpForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
    if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("StreamDeckMidiButton::KeyUpForAction()");

    try
    {
        if (inAction == SEND_NOTE_ON)
        {
            if (mGlobalSettings.printDebug) mConnectionManager->LogMessage(inAction);
            
            if (storedButtonSettings[inContext].noteOffParams == 2)//(noteOffParams == 2)
            {
                SendNoteOff(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiNote, storedButtonSettings[inContext].midiVelocity);
            }
        }
        else if (inAction != SEND_NOTE_ON) return;
    }
    catch (std::exception e)
    {
        mConnectionManager->LogMessage(e.what());
    }
}

void StreamDeckMidiButton::WillAppearForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
    if (!mGlobalSettings.initialSetup)
    {
        mConnectionManager->LogMessage("Requesting global settings from WillAppear");
        mConnectionManager->GetGlobalSettings();
        mConnectionManager->LogMessage("Calling InitialiseMidi()");
        if (InitialiseMidi(Direction::INPUT))
        {
            mConnectionManager->LogMessage("MIDI input initialised successfully");
        }
        else
        {
            mConnectionManager->LogMessage("Something went wrong initialising MIDI input");
            return;
        }
        if (InitialiseMidi(Direction::OUTPUT))
        {
            mConnectionManager->LogMessage("MIDI output initialised successfully");
        }
        else
        {
            mConnectionManager->LogMessage("Something went wrong initialising MIDI output");
            return;
        }
        mGlobalSettings.initialSetup = true;
    }
    if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("WillAppearForAction() - setting the storedButtonSettings for button: " + inContext);
    if (inAction == SEND_CC_TOGGLE) mConnectionManager->LogMessage(inPayload.dump().c_str());
    StoreButtonSettings(inAction, inContext, inPayload, inDeviceID);
    
	// Remember the context
//	mVisibleContextsMutex.lock();
//	mVisibleContexts.insert(inContext);
//	mVisibleContextsMutex.unlock();
}

void StreamDeckMidiButton::WillDisappearForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
	// Remove the context
//	mVisibleContextsMutex.lock();
//	mVisibleContexts.erase(inContext);
//	mVisibleContextsMutex.unlock();
}

void StreamDeckMidiButton::DeviceDidConnect(const std::string& inDeviceID, const json &inDeviceInfo)
{
    mConnectionManager->LogMessage("Device did appear - not software");
}

void StreamDeckMidiButton::DeviceDidDisconnect(const std::string& inDeviceID)
{
	// Nothing to do
}

void StreamDeckMidiButton::SendToPlugin(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
    if (mGlobalSettings.printDebug)
    {
        mConnectionManager->LogMessage("SendToPlugin() callback");
        mConnectionManager->LogMessage(inPayload.dump().c_str());
    }
    if (inAction == SEND_MMC)
    {
        SetActionIcon(inAction, inContext, inPayload, inDeviceID);
    }
    
    const auto event = EPLJSONUtils::GetStringByName(inPayload, "event");
    if (event == "getMidiPorts")
    {
        if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("Trying to get list of Output MIDI ports");
        std::map <std::string, std::string> midiPortList = GetMidiPortList(Direction::OUTPUT);
        if (mGlobalSettings.printDebug) mConnectionManager->LogMessage(json({{"event", "midiOutPorts"},{"midiOutPortList", midiPortList}}).dump().c_str());
        mConnectionManager->SendToPropertyInspector(inAction, inContext,json({{"event", "midiOutPorts"},{"midiOutPortList", midiPortList}}));
        if (mGlobalSettings.printDebug) mConnectionManager->LogMessage(json({{"event", "midiOutPortSelected"},{"midiOutPortSelected", mGlobalSettings.selectedOutPortIndex}}).dump().c_str());
        mConnectionManager->SendToPropertyInspector(inAction, inContext,json({{"event", "midiOutPortSelected"},{"midiOutPortSelected", mGlobalSettings.selectedOutPortIndex}}));
        
        if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("Trying to get list of Input MIDI ports");
        midiPortList = GetMidiPortList(Direction::INPUT);
        if (mGlobalSettings.printDebug) mConnectionManager->LogMessage(json({{"event", "midiInPorts"},{"midiInPortList", midiPortList}}).dump().c_str());
        mConnectionManager->SendToPropertyInspector(inAction, inContext,json({{"event", "midiInPorts"},{"midiInPortList", midiPortList}}));
        if (mGlobalSettings.printDebug) mConnectionManager->LogMessage(json({{"event", "midiInPortSelected"},{"midiInPortSelected", mGlobalSettings.selectedInPortIndex}}).dump().c_str());
        mConnectionManager->SendToPropertyInspector(inAction, inContext,json({{"event", "midiInPortSelected"},{"midiInPortSelected", mGlobalSettings.selectedInPortIndex}}));
        return;

    }
    else
    {
        //something's gone wrong
        if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("something's gone wrong - not expecting this message to be sent to SendToPlugin()");
    }
}

void StreamDeckMidiButton::DidReceiveGlobalSettings(const json& inPayload)
{
    //check to see whether the DEBUG flag is set - should also check whether these have changed
    if (inPayload["settings"].find("printDebug") != inPayload["settings"].end())
    {
        if (mGlobalSettings.printDebug != inPayload["settings"]["printDebug"])
        {
            //printDebug has changed - update
            mGlobalSettings.printDebug = inPayload["settings"]["printDebug"];
            if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("mGlobalSettings.printDebug is set to " + BoolToString(mGlobalSettings.printDebug) + " - will print all debug messages");
            else if (!mGlobalSettings.printDebug) mConnectionManager->LogMessage("mGlobalSettings.printDebug is set to " + BoolToString(mGlobalSettings.printDebug) + " - won't print any debug messages");
        }
    }
    
    //check to see whether we're using the virtual port
    if (inPayload["settings"].find("useVirtualPort") != inPayload["settings"].end())
    {
        if (!inPayload["settings"]["useVirtualPort"])
        {
            //not using the virtual port
            mConnectionManager->LogMessage("not using the virtual port - open physical port");
            mGlobalSettings.useVirtualPort = false;
            
            mGlobalSettings.selectedOutPortIndex = inPayload["settings"]["selectedOutPortIndex"];
            mGlobalSettings.selectedInPortIndex = inPayload["settings"]["selectedInPortIndex"];
            if (mGlobalSettings.printDebug)
            {
                mConnectionManager->LogMessage("Selected OUTPUT port index is " + std::to_string(mGlobalSettings.selectedOutPortIndex) + " & selected port name is " + midiOut->getPortName(mGlobalSettings.selectedOutPortIndex));
                mConnectionManager->LogMessage("Selected INPUT port index is " + std::to_string(mGlobalSettings.selectedInPortIndex) + " & selected port name is " + midiOut->getPortName(mGlobalSettings.selectedInPortIndex));
            }
            
            //reinitialise the midi port here with the correct name
            if (InitialiseMidi(Direction::INPUT))
            {
                mConnectionManager->LogMessage("MIDI input re-initialised successfully");
            }
            else
            {
                mConnectionManager->LogMessage("Something went wrong re-initialising MIDI input");
                return;
            }
            if (InitialiseMidi(Direction::OUTPUT))
            {
                mConnectionManager->LogMessage("MIDI output re-initialised successfully");
            }
            else
            {
                mConnectionManager->LogMessage("Something went wrong re-initialising MIDI output");
                return;
            }
        }
        else if (inPayload["settings"]["useVirtualPort"])//if useVirtualPort has been set to true
        {
            if (mGlobalSettings.useVirtualPort == inPayload["settings"]["useVirtualPort"])
            {
                //useVirtualPort hasn't changed - has the portName changed?
                if (mGlobalSettings.portName != inPayload["settings"]["portName"])
                {
                    //the portName has changed and we're using the virtual port - reinitialise MIDI with the correct portName
                    mGlobalSettings.portName = inPayload["settings"]["portName"];
                    if (InitialiseMidi(Direction::INPUT))
                    {
                        mConnectionManager->LogMessage("MIDI input re-initialised successfully");
                    }
                    else
                    {
                        mConnectionManager->LogMessage("Something went wrong re-initialising MIDI input");
                        return;
                    }
                    if (InitialiseMidi(Direction::OUTPUT))
                    {
                        mConnectionManager->LogMessage("MIDI output re-initialised successfully");
                    }
                    else
                    {
                        mConnectionManager->LogMessage("Something went wrong re-initialising MIDI output");
                        return;
                    }
                }
                else if (mGlobalSettings.portName == inPayload["settings"]["portName"])
                {
                    if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("Virtual Port name hasn't changed - not re-initialising");
                    return;
                }
            }
            else if (mGlobalSettings.useVirtualPort != inPayload["settings"]["useVirtualPort"])
            {
                //useVirtualPort has changed
                mGlobalSettings.useVirtualPort = true;
                //re-initialise MIDI with the virtual port
                if (InitialiseMidi(Direction::INPUT))
                {
                    mConnectionManager->LogMessage("MIDI input re-initialised successfully");
                }
                else
                {
                    mConnectionManager->LogMessage("Something went wrong re-initialising MIDI input");
                    return;
                }
                if (InitialiseMidi(Direction::OUTPUT))
                {
                    mConnectionManager->LogMessage("MIDI output re-initialised successfully");
                }
                else
                {
                    mConnectionManager->LogMessage("Something went wrong re-initialising MIDI output");
                    return;
                }
            }
        }
    }
}

void StreamDeckMidiButton::DidReceiveSettings(const std::string& inAction, const std::string& inContext, const json& inPayload, const std::string& inDeviceID)
{
    if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("didReceiveSettings() - setting the storedButtonSettings for button: " + inContext);
    StoreButtonSettings(inAction, inContext, inPayload, inDeviceID);
}

void StreamDeckMidiButton::SendNoteOn(const int midiChannel, const int midiNote, const int midiVelocity, const int noteOffParams)
{
    if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("StreamDeckMidiButton::SendNoteOn()");

    //Note On message
    std::vector<unsigned char> midiMessage;
    midiMessage.push_back(143 + midiChannel);
    midiMessage.push_back(midiNote);
    midiMessage.push_back(midiVelocity);
    midiOut->sendMessage(&midiMessage);

    switch (noteOffParams)
    {
        case 0: {//no note off message
            if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("no note off message");
            break;
        }
        case PUSH_NOTE_OFF: {//1: {//send note off message immediately
            if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("send note off immediately");
            SendNoteOff(midiChannel, midiNote, midiVelocity);
            break;
        }
        case RELEASE_NOTE_OFF: {//2: {//send note off on KeyUp
            if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("send note off on KeyUp");
            break;
        }
        default: {
            break;
        }
    }
}

void StreamDeckMidiButton::SendNoteOff(const int midiChannel, const int midiNote, const int midiVelocity)
{
    if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("StreamDeckMidiButton::SendNoteOff()");
    
    //Note Off message
    std::vector<unsigned char> midiMessage;
    midiMessage.push_back(127 + midiChannel);
    midiMessage.push_back(midiNote);
    midiMessage.push_back(midiVelocity);
    midiOut->sendMessage(&midiMessage);
}

void StreamDeckMidiButton::SendCC(const int midiChannel, const int midiCC, const int midiValue)
{
    if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("StreamDeckMidiButton::SendCC()");
    
    //midi CC message
    std::vector<unsigned char> midiMessage;
    midiMessage.push_back(175 + midiChannel);
    midiMessage.push_back(midiCC);
    midiMessage.push_back(midiValue);
    midiOut->sendMessage(&midiMessage);
}

void StreamDeckMidiButton::SendMMC(const int sendMMC)
{
    if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("StreamDeckMidiButton::SendMMC()");
    
    //Note On message
    std::vector<unsigned char> midiMessage;
    midiMessage.push_back(240); //F0 hex
    midiMessage.push_back(127); //7F hex
    midiMessage.push_back(127); //7F hex - device-id all devices
    midiMessage.push_back(6); //6 - sub-id #1 command
    midiMessage.push_back(sendMMC); //send the MMC command
    midiMessage.push_back(247); //F7 - message end
    midiOut->sendMessage(&midiMessage);
    
}

void StreamDeckMidiButton::SendProgramChange(const int midiChannel, const int midiProgramChange)
{
    if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("StreamDeckMidiButton::SendProgramChange()");
    
    //midi CC message
    std::vector<unsigned char> midiMessage;
    midiMessage.push_back(191 + midiChannel);
    midiMessage.push_back(midiProgramChange);
    midiOut->sendMessage(&midiMessage);

}

void StreamDeckMidiButton::SetActionIcon(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
    if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("StreamDeckMidiButton::SetActionIcon()");
    //get the MMC message
    try
    {
        switch (storedButtonSettings[inContext].midiMMC)
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
        mConnectionManager->LogMessage("Something went wrong setting the action icon - the file is probably missing");
    }
}

std::map <std::string, std::string> StreamDeckMidiButton::GetMidiPortList(Direction direction)
{
    switch (direction)
    {
        case Direction::OUTPUT:
        {
            if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("getting OUTPUT port list");
            std::map <std::string, std::string> midiPortList;
            std::string portName;
            unsigned int nPorts = midiOut->getPortCount();
            if (nPorts == 0)
            {
                midiPortList.insert(std::make_pair("No ports available - this is an error", std::to_string(0)));
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
            
            if (mGlobalSettings.printDebug)
            {
                std::map<std::string, std::string>::iterator it = midiPortList.begin();
                while(it != midiPortList.end())
                {
                    mConnectionManager->LogMessage(it->first + " has port index: " + it->second);
                    it++;
                }
            }
            return midiPortList;
        }
        case Direction::INPUT:
        {
            if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("getting INPUT port list");
            std::map <std::string, std::string> midiPortList;
            std::string portName;
            unsigned int nPorts = midiIn->getPortCount();
            if (nPorts == 0)
            {
                midiPortList.insert(std::make_pair("No ports available - this is an error", std::to_string(0)));
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
            
            if (mGlobalSettings.printDebug)
            {
                std::map<std::string, std::string>::iterator it = midiPortList.begin();
                while(it != midiPortList.end())
                {
                    mConnectionManager->LogMessage(it->first + " has port index: " + it->second);
                    it++;
                }
            }
            return midiPortList;
        }
    }
}

std::string StreamDeckMidiButton::GetPngAsString(const char* filename)
{
    //try reading the file
    std::vector<unsigned char> imageFile = ReadFile(filename);
    if (imageFile.empty())
    {
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
    // open the file:
    std::ifstream file(filename, std::ios::binary);
    std::vector<unsigned char> vec;
    
    if (!filename)
    {
        mConnectionManager->LogMessage("couldn't open file " + std::string(filename));
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

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
    
    //start a new timer and call the function every 1 ms
    mTimer = new CallBackTimer();
    mTimer->start(1, [this]()
    {
        //check for MIDI input and act on it - to be written
        this->GetMidiInput();
        this->UpdateTimer();
        
//        //check each button and see if we need to do a fade
//
//        // Create a map iterator and point to beginning of map
//
//        std::map<std::string, FadeSet>::iterator it = this->storedFadeSettings.begin();
//
//        // Iterate over the map using Iterator till end.
//        while (it != this->storedFadeSettings.end())
//        {
//            if (this->storedFadeSettings[it->first].fadeActive)//it->first is the same as inContext
//            {
//                if (this->storedFadeSettings[it->first].UpdateFade())
//                {
//                    //we have an updated value - send it out as a MIDI CC message
//                    SendCC(storedButtonSettings[it->first].midiChannel, storedButtonSettings[it->first].midiCC, storedFadeSettings[it->first].currentValue);
//                }
//            }
//            if (this->storedFadeSettings[it->first].fadeFinished)
//            {
//                //we have a finished fade - print a TICK to the button
//                mConnectionManager->ShowOKForContext(it->first);
//                this->storedFadeSettings[it->first].fadeFinished = !this->storedFadeSettings[it->first].fadeFinished;
//            }
//            // Increment the Iterator to point to next entry
//            it++;
//        }
    });
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
        if(mTimer != nullptr)
        {
            mTimer->stop();
            delete mTimer;
            mTimer = nullptr;
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
            mConnectionManager->LogMessage("bool StreamDeckMidiButton::InitialiseMidi(Direction OUT)");
            try
            {
                if (midiOut != nullptr)
                {
                    mConnectionManager->LogMessage("bool StreamDeckMidiButton::InitialiseMidi(Direction OUT): midiOut is already initialised - deleting");
                    delete midiOut;
                    mConnectionManager->LogMessage("bool StreamDeckMidiButton::InitialiseMidi(Direction OUT): creating a new midiOut");
                    midiOut = new RtMidiOut();
                }
#if defined (__APPLE__)
                if (mGlobalSettings->useVirtualPort)
                {
                    if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("bool StreamDeckMidiButton::InitialiseMidi(Direction OUT): opening virtual OUTPUT port called: " + mGlobalSettings->portName);
                    midiOut->openVirtualPort(mGlobalSettings->portName);
                }
                else
                {
#endif
                    // open the selected OUTPUT port
                    if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("bool StreamDeckMidiButton::InitialiseMidi(Direction OUT): opening OUTPUT port index: " + std::to_string(mGlobalSettings->selectedOutPortIndex) + " called: " + midiOut->getPortName(mGlobalSettings->selectedOutPortIndex));
                    midiOut->openPort(mGlobalSettings->selectedOutPortIndex);
#if defined (__APPLE__)
                }
#endif
            }
            catch (RtMidiError &error)
            {
                mConnectionManager->LogMessage("bool StreamDeckMidiButton::InitialiseMidi(Direction OUT): problem with RtMidi - exiting with prejudice");
                mConnectionManager->LogMessage(error.getMessage());
                exit(EXIT_FAILURE);
            }
            return true;
        }
        case Direction::IN:
        {
            mConnectionManager->LogMessage("bool StreamDeckMidiButton::InitialiseMidi(Direction IN)");
            try
            {
                if (midiIn != nullptr)
                {
                    mConnectionManager->LogMessage("bool StreamDeckMidiButton::InitialiseMidi(Direction IN): midiIn is already initialised - deleting");
                    delete midiIn;
                    mConnectionManager->LogMessage("bool StreamDeckMidiButton::InitialiseMidi(Direction IN): creating a new midiIn");
                    midiIn = new RtMidiIn();
                }
                
                // don't ignore sysex, timing or active sensing messages
                midiIn->ignoreTypes(false, false, false);
#if defined (__APPLE__)
                if (mGlobalSettings->useVirtualPort)
                {
                    if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("bool StreamDeckMidiButton::InitialiseMidi(Direction IN): opening virtual INPUT port called: " + mGlobalSettings->portName);
                    midiIn->openVirtualPort(mGlobalSettings->portName);
                }
                else
                {
#endif
                    // open the selected INPUT port
                    if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("bool StreamDeckMidiButton::InitialiseMidi(Direction IN): opening INPUT port index: " + std::to_string(mGlobalSettings->selectedInPortIndex) + " called: " + midiOut->getPortName(mGlobalSettings->selectedInPortIndex));
                    midiIn->openPort(mGlobalSettings->selectedInPortIndex);
                }
            }
            catch (RtMidiError &error)
            {
                mConnectionManager->LogMessage("bool StreamDeckMidiButton::InitialiseMidi(Direction IN): problem with RtMidi - exiting with prejudice");
                mConnectionManager->LogMessage(error.getMessage());
                exit(EXIT_FAILURE);
            }
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
    /*
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
    }*/
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
                SendCC(storedButtonSettings[it->first].midiChannel, storedButtonSettings[it->first].midiCC, storedFadeSettings[it->first].currentValue);
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
    mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings()");
    if (mGlobalSettings->printDebug)
    {
        mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): received button settings for " + inContext + ", with action " + inAction + ": " + inPayload.dump().c_str());
    }
    
    ButtonSettings thisButtonSettings;
    
    //should iterate through the JSON payload here and store everything into a struct
    if (inPayload["settings"].find("midiChannel") != inPayload["settings"].end())
    {
        thisButtonSettings.midiChannel = inPayload["settings"]["midiChannel"];
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): Setting midiChannel to " + std::to_string(thisButtonSettings.midiChannel));
    }
    if (inPayload["settings"].find("midiNote") != inPayload["settings"].end())
    {
        thisButtonSettings.midiNote = inPayload["settings"]["midiNote"];
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): Setting midiNote to " + std::to_string(thisButtonSettings.midiNote));
    }
    if (inPayload["settings"].find("midiVelocity") != inPayload["settings"].end())
    {
        thisButtonSettings.midiVelocity = inPayload["settings"]["midiVelocity"];
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): Setting midiVelocity to " + std::to_string(thisButtonSettings.midiVelocity));
    }
    if (inPayload["settings"].find("noteOffMode") != inPayload["settings"].end())
    {
        thisButtonSettings.noteOffMode = inPayload["settings"]["noteOffMode"];
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): Setting noteOffMode to " + std::to_string(thisButtonSettings.noteOffMode));
    }
    if (inPayload["settings"].find("midiProgramChange") != inPayload["settings"].end())
    {
        thisButtonSettings.midiProgramChange = inPayload["settings"]["midiProgramChange"];
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): Setting midiProgramChange to " + std::to_string(thisButtonSettings.midiProgramChange));
    }
    if (inPayload["settings"].find("midiCC") != inPayload["settings"].end())
    {
        thisButtonSettings.midiCC = inPayload["settings"]["midiCC"];
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): Setting midiCC to " + std::to_string(thisButtonSettings.midiCC));
    }
    if (inPayload["settings"].find("midiValue") != inPayload["settings"].end())
    {
        thisButtonSettings.midiValue = inPayload["settings"]["midiValue"];
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): Setting midiValue to " + std::to_string(thisButtonSettings.midiValue));
    }
    if (inPayload["settings"].find("midiValueSec") != inPayload["settings"].end())
    {
        thisButtonSettings.midiValueSec = inPayload["settings"]["midiValueSec"];
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): Setting midiValueSec to " + std::to_string(thisButtonSettings.midiValueSec));
    }
    if (inPayload["settings"].find("midiMMC") != inPayload["settings"].end())
    {
        thisButtonSettings.midiMMC = inPayload["settings"]["midiMMC"];
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): Setting midiMMC to " + std::to_string(thisButtonSettings.midiMMC));
    }
    if (inPayload["settings"].find("toggleFade") != inPayload["settings"].end())
    {
        thisButtonSettings.toggleFade = inPayload["settings"]["toggleFade"];
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): Setting toggleFade to " + BoolToString(thisButtonSettings.toggleFade));
    }
    if (inPayload["settings"].find("fadeTime") != inPayload["settings"].end())
    {
        thisButtonSettings.fadeTime = inPayload["settings"]["fadeTime"];
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): Setting fadeTime to " + std::to_string(thisButtonSettings.fadeTime));
    }
    if (inPayload["settings"].find("fadeCurve") != inPayload["settings"].end())
    {
        thisButtonSettings.fadeCurve = inPayload["settings"]["fadeCurve"];
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): Setting fadeCurve to " + std::to_string(thisButtonSettings.fadeCurve));
    }
    if (inPayload["settings"].find("ccMode") != inPayload["settings"].end())
    {
        thisButtonSettings.ccMode = inPayload["settings"]["ccMode"];
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): Setting ccMode to " + std::to_string(thisButtonSettings.ccMode));
        //if ccMode is 2 or 3 set the toggleFade flag
        if (thisButtonSettings.ccMode == 2 || thisButtonSettings.ccMode == 3)
        {
            thisButtonSettings.toggleFade = true;
            if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): ccMode is 2 or 3 - setting toggleFade to " + BoolToString(thisButtonSettings.toggleFade));
        }
        else
        {
            thisButtonSettings.toggleFade = false;
            if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): ccMode is 0 or 1 - setting toggleFade to " + BoolToString(thisButtonSettings.toggleFade));
        }
    }

    //store everything into the map
    if (storedButtonSettings.insert(std::make_pair(inContext, thisButtonSettings)).second == false)
    {
        //key already exists, so replace it
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): Key already exists - replacing");
        storedButtonSettings[inContext] = thisButtonSettings;
    }
    
    if (mGlobalSettings->printDebug)//check it's been stored correctly
    {
        //iterate through settings and log messages - should be a way to do this in a loop
        mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): midiChannel for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].midiChannel));
        mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): midiNote for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].midiNote));
        mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): midiVelocity for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].midiVelocity));
        mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): noteOffMode for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].noteOffMode));
        mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): midiProgramChange for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].midiProgramChange));
        mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): midiCC for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].midiCC));
        mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): midiValue for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].midiValue));
        mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): midiValueSec for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].midiValueSec));
        mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): midiMMC for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].midiMMC));
        mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): toggleFade for " + inContext + " is set to: " + BoolToString(storedButtonSettings[inContext].toggleFade));
        mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): fadeTime for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].fadeTime));
        mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): fadeCurve for " + inContext + " is set to: " + std::to_string(storedButtonSettings[inContext].fadeCurve));
    }

    //if it's an MMC button set the icon
    if (inAction == SEND_MMC)
    {
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): inAction == SEND_MMC -> SetActionIcon()");
        SetActionIcon(inAction, inContext, inPayload, inDeviceID);
    }
    
    //if it's a CC Toggle button generate the fadeSet, if required
    if (storedButtonSettings[inContext].toggleFade)//create the fadeSet for the button
    {
        if (storedButtonSettings[inContext].fadeTime == 0)
        {
            if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): We have a fadeTime of 0! - divide by zero error, so ignoring by switching toggleFade off");
            storedButtonSettings[inContext].toggleFade = false; //to avoid divide by zero problem
        }
        else
        {
            if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): Generating the fade lookup table");
            FadeSet thisButtonFadeSet(storedButtonSettings[inContext].midiValue, storedButtonSettings[inContext].midiValueSec, storedButtonSettings[inContext].fadeTime, storedButtonSettings[inContext].fadeCurve, mGlobalSettings->sampleInterval);
            if (storedFadeSettings.insert(std::make_pair(inContext, thisButtonFadeSet)).second == false)
            {
                //key already exists, so replace it
                if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::StoreButtonSettings(): FadeSet already exists - replacing");
                storedFadeSettings[inContext] = thisButtonFadeSet;
            }
        }
    }
}

void StreamDeckMidiButton::KeyDownForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
    mConnectionManager->LogMessage("void StreamDeckMidiButton::KeyDownForAction()");
        
    if (mGlobalSettings->printDebug)
    {
        mConnectionManager->LogMessage("void StreamDeckMidiButton::KeyDownForAction(): dumping the JSON payload: " + inPayload.dump());
        //mConnectionManager->LogMessage(inPayload.dump().c_str());//REALLY USEFUL for debugging
    }
    
    if (storedButtonSettings.find(inContext) == storedButtonSettings.end())//something's gone wrong - no settings stored, so the PI probably hasn't been opened
    {
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::KeyDownForAction(): No storedButtonSettings - something went wrong");
        StoreButtonSettings(inAction, inContext, inPayload, inDeviceID);
    }

    try
    {
        if (inAction == SEND_NOTE_ON)
        {
            if (mGlobalSettings->printDebug)
            {
                mConnectionManager->LogMessage("void StreamDeckMidiButton::KeyDownForAction(): inAction: " + inAction);
                mConnectionManager->LogMessage("void StreamDeckMidiButton::KeyDownForAction(): inContext: " + inContext);
            }
            SendNoteOn(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiNote, storedButtonSettings[inContext].midiVelocity, storedButtonSettings[inContext].noteOffMode);
        }
        else if (inAction == SEND_NOTE_ON_TOGGLE)
        {
            if (mGlobalSettings->printDebug)
            {
                mConnectionManager->LogMessage("void StreamDeckMidiButton::KeyDownForAction(): inAction: " + inAction);
                mConnectionManager->LogMessage("void StreamDeckMidiButton::KeyDownForAction(): inContext: " + inContext);
                if (inPayload.contains("state")) mConnectionManager->LogMessage("void StreamDeckMidiButton::KeyDownForAction(): we have a state of " + std::to_string(inPayload["state"].get<int>()));
                if (inPayload.contains("userDesiredState")) mConnectionManager->LogMessage("void StreamDeckMidiButton::KeyDownForAction(): we have a userDesiredState of " + std::to_string(inPayload["userDesiredState"].get<int>()));
            }
            if (inPayload.contains("state"))
            {
                if (inPayload.contains("userDesiredState"))
                {
                    if (inPayload["userDesiredState"].get<int>() == 0)
                    {
                        SendNoteOn(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiNote, storedButtonSettings[inContext].midiVelocity);
                    }
                    else if (inPayload["userDesiredState"].get<int>() == 1)
                    {
                        SendNoteOff(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiNote, storedButtonSettings[inContext].midiVelocity);
                    }
                }
                else
                {
                    if (inPayload["state"].get<int>() == 0)
                    {
                        SendNoteOn(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiNote, storedButtonSettings[inContext].midiVelocity);
                    }
                    else if (inPayload["state"].get<int>() == 1)
                    {
                        SendNoteOff(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiNote, storedButtonSettings[inContext].midiVelocity);
                    }
                }
            }
            else mConnectionManager->LogMessage("void StreamDeckMidiButton::KeyDownForAction(): something went wrong - should have a state, and we don't have");
        }
        else if (inAction == SEND_CC)
        {
            if (mGlobalSettings->printDebug)
            {
                mConnectionManager->LogMessage("void StreamDeckMidiButton::KeyDownForAction(): inAction: " + inAction);
                mConnectionManager->LogMessage("void StreamDeckMidiButton::KeyDownForAction(): inContext: " + inContext);
            }
            switch (storedButtonSettings[inContext].ccMode)
            {
                case 0: case 1://single CC value, or momentary without fade
                    SendCC(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiCC, storedButtonSettings[inContext].midiValue);
                    break;
                case 2://fade IN until button is released, and then fade IN - KeyUpForAction()
                    storedFadeSettings[inContext].currentDirection = Direction::IN;
                    storedFadeSettings[inContext].fadeActive = true;
                    break;
                case 3://fade OUT until button is released, and then fade IN - KeyUpForAction()
                    storedFadeSettings[inContext].currentDirection = Direction::OUT;
                    storedFadeSettings[inContext].fadeActive = true;
                    break;
            }
        }
        else if (inAction == SEND_CC_TOGGLE)
        {
            if (mGlobalSettings->printDebug)
            {
                mConnectionManager->LogMessage("void StreamDeckMidiButton::KeyDownForAction(): inAction: " + inAction);
                mConnectionManager->LogMessage("void StreamDeckMidiButton::KeyDownForAction(): inContext: " + inContext);
                if (inPayload.contains("state")) mConnectionManager->LogMessage("void StreamDeckMidiButton::KeyDownForAction(): we have a state of " + std::to_string(inPayload["state"].get<int>()));
                if (inPayload.contains("userDesiredState")) mConnectionManager->LogMessage("void StreamDeckMidiButton::KeyDownForAction(): we have a userDesiredState of " + std::to_string(inPayload["userDesiredState"].get<int>()));
            }
            if (inPayload.contains("state"))
            {
                if (inPayload.contains("userDesiredState"))
                {
                    if (inPayload["userDesiredState"].get<int>() == 0)
                    {
                        if (!storedButtonSettings[inContext].toggleFade) SendCC(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiCC, storedButtonSettings[inContext].midiValue);
                        else if (storedButtonSettings[inContext].toggleFade)
                        {
                            if (storedFadeSettings[inContext].fadeActive) storedFadeSettings[inContext].ReverseFade();
                            else
                            {
                                storedFadeSettings[inContext].currentDirection = Direction::IN;
                                storedFadeSettings[inContext].fadeActive = true;
                            }
                        }
                    }
                    else if (inPayload["userDesiredState"].get<int>() == 1)
                    {
                        if (!storedButtonSettings[inContext].toggleFade) SendCC(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiCC, storedButtonSettings[inContext].midiValueSec);
                        else if (storedButtonSettings[inContext].toggleFade)
                        {
                            if (storedFadeSettings[inContext].fadeActive) storedFadeSettings[inContext].ReverseFade();
                            else
                            {
                                storedFadeSettings[inContext].currentDirection = Direction::OUT;
                                storedFadeSettings[inContext].fadeActive = true;
                            }
                        }
                    }
                }
                else
                {
                    if (inPayload["state"].get<int>() == 0)
                    {
                        if (!storedButtonSettings[inContext].toggleFade) SendCC(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiCC, storedButtonSettings[inContext].midiValue);
                        else if (storedButtonSettings[inContext].toggleFade)
                        {
                            if (storedFadeSettings[inContext].fadeActive) storedFadeSettings[inContext].ReverseFade();
                            else
                            {
                                storedFadeSettings[inContext].currentDirection = Direction::IN;
                                storedFadeSettings[inContext].fadeActive = true;
                            }
                        }
                    }
                    else if (inPayload["state"].get<int>() == 1)
                    {
                        if (!storedButtonSettings[inContext].toggleFade) SendCC(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiCC, storedButtonSettings[inContext].midiValueSec);
                        else if (storedButtonSettings[inContext].toggleFade)
                        {
                            if (storedFadeSettings[inContext].fadeActive) storedFadeSettings[inContext].ReverseFade();
                            else
                            {
                                storedFadeSettings[inContext].currentDirection = Direction::OUT;
                                storedFadeSettings[inContext].fadeActive = true;
                            }
                        }
                    }
                }
            }
            else mConnectionManager->LogMessage("void StreamDeckMidiButton::KeyDownForAction(): Something's gone wrong - should have a state, and we don't have");
        }
        else if (inAction == SEND_PROGRAM_CHANGE)
        {
            if (mGlobalSettings->printDebug) mConnectionManager->LogMessage(inAction);
            SendProgramChange(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiProgramChange);
        }
        else if (inAction == SEND_MMC)
        {
            if (mGlobalSettings->printDebug) mConnectionManager->LogMessage(inAction);
            SendMMC(storedButtonSettings[inContext].midiMMC);
        }
        else
        {
            //something has gone wrong
            mConnectionManager->LogMessage("void StreamDeckMidiButton::KeyDownForAction(): Something's gone wrong - no inAction sent, which shouldn't be possible");
        }
    }
    catch (std::exception e)
    {
        mConnectionManager->LogMessage("void StreamDeckMidiButton::KeyDownForAction(): Something's gone wrong - an unknown error has occurred as below");
        mConnectionManager->LogMessage(e.what());
    }
}

void StreamDeckMidiButton::KeyUpForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
    mConnectionManager->LogMessage("void StreamDeckMidiButton::KeyUpForAction()");
    try
    {
        if (inAction == SEND_NOTE_ON)
        {
            if (storedButtonSettings[inContext].noteOffMode == 2)//(noteOffMode == 2)
            {
                if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::KeyUpForAction(): send note off");
                SendNoteOff(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiNote, storedButtonSettings[inContext].midiVelocity);
            }
        }
        else if (inAction == SEND_CC)
        {
            switch (storedButtonSettings[inContext].ccMode)
            {
                case 0://single CC value only
                    break;
                case 1://momentary without fade
                    if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::KeyUpForAction(): send secondary CC value");
                    SendCC(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiCC, storedButtonSettings[inContext].midiValueSec);
                    break;
                case 2://fade OUT after fading IN
                    if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::KeyUpForAction(): ReverseFade()");
                    storedFadeSettings[inContext].ReverseFade();
                    break;
                case 3://fade IN after fading OUT
                    if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::KeyUpForAction(): ReverseFade()");
                    storedFadeSettings[inContext].ReverseFade();
                    break;
            }
        }
        else return;
    }
    catch (std::exception e)
    {
        mConnectionManager->LogMessage("void StreamDeckMidiButton::KeyUpForAction(): something went wrong - error message follows");
        mConnectionManager->LogMessage(e.what());
    }
}

void StreamDeckMidiButton::WillAppearForAction(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
    mConnectionManager->LogMessage("void StreamDeckMidiButton::WillAppearForAction()");
    
    if (!mGlobalSettings->initialSetup)
    {
        mConnectionManager->GetGlobalSettings();
    }
    
    //lock the context
    mVisibleContextsMutex.lock();
    mVisibleContexts.insert(inContext);
    
    //store the button settings
    if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::WillAppearForAction(): setting the storedButtonSettings for button: " + inContext);
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
    mConnectionManager->LogMessage("void StreamDeckMidiButton::DeviceDidConnect()");
}

void StreamDeckMidiButton::DeviceDidDisconnect(const std::string& inDeviceID)
{
	// Nothing to do
}

void StreamDeckMidiButton::SendToPlugin(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
    if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::SendToPlugin(): " + inPayload.dump());
    else mConnectionManager->LogMessage("void StreamDeckMidiButton::SendToPlugin()");
    
    if (inAction == SEND_MMC)
    {
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::SendToPlugin(): new MMC action - set the action icon");
        SetActionIcon(inAction, inContext, inPayload, inDeviceID);
    }
    
    const auto event = EPLJSONUtils::GetStringByName(inPayload, "event");
    if (event == "getMidiPorts")
    {
        std::map <std::string, std::string> midiPortList = GetMidiPortList(Direction::OUT);
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::SendToPlugin(): get list of MIDI OUT ports - " + json({{"event", "midiOutPorts"},{"midiOutPortList", midiPortList}}).dump());
        mConnectionManager->SendToPropertyInspector(inAction, inContext,json({{"event", "midiOutPorts"},{"midiOutPortList", midiPortList}}));
        
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::SendToPlugin(): selected MIDI OUT port - " + json({{"event", "midiOutPortSelected"},{"midiOutPortSelected", mGlobalSettings->selectedOutPortIndex}}).dump());
        mConnectionManager->SendToPropertyInspector(inAction, inContext,json({{"event", "midiOutPortSelected"},{"midiOutPortSelected", mGlobalSettings->selectedOutPortIndex}}));
        
        midiPortList = GetMidiPortList(Direction::IN);
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::SendToPlugin(): get list of MIDI IN ports - " + json({{"event", "midiInPorts"},{"midiInPortList", midiPortList}}).dump());
        mConnectionManager->SendToPropertyInspector(inAction, inContext,json({{"event", "midiInPorts"},{"midiInPortList", midiPortList}}));
        
        if (mGlobalSettings->printDebug) mConnectionManager->LogMessage(json({{"event", "midiInPortSelected"},{"midiInPortSelected", mGlobalSettings->selectedInPortIndex}}).dump().c_str());
        mConnectionManager->SendToPropertyInspector(inAction, inContext,json({{"event", "midiInPortSelected"},{"midiInPortSelected", mGlobalSettings->selectedInPortIndex}}));
        return;
    }
    //else something's gone wrong
    else mConnectionManager->LogMessage("void StreamDeckMidiButton::SendToPlugin(): something went wrong - not expecting this message to be sent. Dumping payload: " + inPayload.dump());
}

void StreamDeckMidiButton::DidReceiveGlobalSettings(const json& inPayload)
{
    mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): inPayload = " + inPayload.dump());
    if (!mGlobalSettings->initialSetup)//we haven't done the initial setup
    {
        //log that we haven't done the initial setup
        mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): mGlobalSettings->initialSetup == " + BoolToString(mGlobalSettings->initialSetup) + " - perform initial setup");
        //see if the printDebug flag has been set
        if (inPayload["settings"].find("printDebug") != inPayload["settings"].end())
        {
            mGlobalSettings->printDebug = inPayload["settings"]["printDebug"];
            if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): mGlobalSettings->printDebug is set to " + BoolToString(mGlobalSettings->printDebug) + " - will print all debug messages");
            else if (!mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): mGlobalSettings->printDebug is set to " + BoolToString(mGlobalSettings->printDebug) + " - won't print verbose debug messages");
        }
        else mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): inPayload[\"settings\"][\"printDebug\"] is empty - setting to default == true");
        //see if the virtual port flag has been set
        if (inPayload["settings"].find("useVirtualPort") != inPayload["settings"].end()) mGlobalSettings->useVirtualPort = inPayload["settings"]["useVirtualPort"];
        //get the portName, if that's been set
        if (inPayload["settings"].find("portName") != inPayload["settings"].end()) mGlobalSettings->portName = inPayload["settings"]["portName"];
        //get the selectedPortIndexes, if set
        if (inPayload["settings"].find("selectedOutPortIndex") != inPayload["settings"].end()) mGlobalSettings->selectedOutPortIndex = inPayload["settings"]["selectedOutPortIndex"];
        if (inPayload["settings"].find("selectedInPortIndex") != inPayload["settings"].end()) mGlobalSettings->selectedInPortIndex = inPayload["settings"]["selectedInPortIndex"];
        
        if (mGlobalSettings->useVirtualPort)
        {
            if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): opening the virtual port with portName " + mGlobalSettings->portName);
            if (InitialiseMidi(Direction::IN))
            {
                mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): MIDI input initialised successfully");
            }
            else
            {
                mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): something went wrong initialising MIDI input");
                return;
            }
            if (InitialiseMidi(Direction::OUT))
            {
                mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): MIDI output initialised successfully");
            }
            else
            {
                mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): something went wrong initialising MIDI output");
                return;
            }
        }
        else if (!mGlobalSettings->useVirtualPort)
        {
            if (mGlobalSettings->printDebug)
            {
                mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): opening the physical OUTPUT port with index " + std::to_string(mGlobalSettings->selectedOutPortIndex) + " & port name " + midiOut->getPortName(mGlobalSettings->selectedOutPortIndex));
                mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): opening the physical INPUT port with index " + std::to_string(mGlobalSettings->selectedInPortIndex) + " & port name " + midiOut->getPortName(mGlobalSettings->selectedInPortIndex));
            }
            if (InitialiseMidi(Direction::IN))
            {
                mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): MIDI input initialised successfully");
            }
            else
            {
                mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): something went wrong initialising MIDI input");
                return;
            }
            if (InitialiseMidi(Direction::OUT))
            {
                mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): MIDI output initialised successfully");
            }
            else
            {
                mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): something went wrong initialising MIDI output");
                return;
            }

        }
        mGlobalSettings->initialSetup = true;
    }
    else if (mGlobalSettings->initialSetup)
    {
        //check the printDebug flag, and if it has changed update
        if (inPayload["settings"].find("printDebug") != inPayload["settings"].end() && mGlobalSettings->printDebug != inPayload["settings"]["printDebug"])//printDebug flag is present and has changed - update
        {
            mGlobalSettings->printDebug = inPayload["settings"]["printDebug"];
            if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): mGlobalSettings->printDebug is set to " + BoolToString(mGlobalSettings->printDebug) + " - will print all debug messages");
            else if (!mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): mGlobalSettings->printDebug is set to " + BoolToString(mGlobalSettings->printDebug) + " - won't print any debug messages");
        }
        
        //check to see whether we're using the virtual port, and if it has changed, update
        if (inPayload["settings"].find("useVirtualPort") != inPayload["settings"].end() && mGlobalSettings->useVirtualPort != inPayload["settings"]["useVirtualPort"])//useVirtualPort flag has changed - update
        {
            mGlobalSettings->useVirtualPort = inPayload["settings"]["useVirtualPort"];
            if (!mGlobalSettings->useVirtualPort)
            {
                //not using the virtual port
                mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): not using the virtual port - open physical port");
                mGlobalSettings->useVirtualPort = false;
                
                if (inPayload["settings"].find("selectedOutPortIndex") != inPayload["settings"].end()) mGlobalSettings->selectedOutPortIndex = inPayload["settings"]["selectedOutPortIndex"];
                if (inPayload["settings"].find("selectedInPortIndex") != inPayload["settings"].end()) mGlobalSettings->selectedInPortIndex = inPayload["settings"]["selectedInPortIndex"];
                
                if (mGlobalSettings->printDebug)
                {
                    mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): opening the physical OUTPUT port with index " + std::to_string(mGlobalSettings->selectedOutPortIndex) + " & port name " + midiOut->getPortName(mGlobalSettings->selectedOutPortIndex));
                    mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): opening the physical INPUT port with index " + std::to_string(mGlobalSettings->selectedInPortIndex) + " & port name " + midiOut->getPortName(mGlobalSettings->selectedInPortIndex));
                }
                
                //reinitialise the midi port here with the correct name
                if (InitialiseMidi(Direction::IN))
                {
                    mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): MIDI input re-initialised successfully");
                }
                else
                {
                    mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): something went wrong re-initialising MIDI input");
                    return;
                }
                if (InitialiseMidi(Direction::OUT))
                {
                    mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): MIDI output re-initialised successfully");
                }
                else
                {
                    mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): something went wrong re-initialising MIDI output");
                    return;
                }
            }
            else if (mGlobalSettings->useVirtualPort)//if useVirtualPort has been set to true
            {
                if (mGlobalSettings->portName != inPayload["settings"]["portName"])
                {
                    //the portName has changed and we're using the virtual port - reinitialise MIDI with the correct portName
                    mGlobalSettings->portName = inPayload["settings"]["portName"];
                    if (InitialiseMidi(Direction::IN))
                    {
                        mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): MIDI input re-initialised successfully");
                    }
                    else
                    {
                        mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): something went wrong re-initialising MIDI input");
                        return;
                    }
                    if (InitialiseMidi(Direction::OUT))
                    {
                        mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): MIDI output re-initialised successfully");
                    }
                    else
                    {
                        mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): something went wrong re-initialising MIDI output");
                        return;
                    }
                }
                else if (mGlobalSettings->portName == inPayload["settings"]["portName"])
                {
                    if (InitialiseMidi(Direction::IN))
                    {
                        mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): MIDI input re-initialised successfully");
                    }
                    else
                    {
                        mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): something went wrong re-initialising MIDI input");
                        return;
                    }
                    if (InitialiseMidi(Direction::OUT))
                    {
                        mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): MIDI output re-initialised successfully");
                    }
                    else
                    {
                        mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveGlobalSettings(): something went wrong re-initialising MIDI output");
                        return;
                    }
                }
            }
        }
    }
}

void StreamDeckMidiButton::DidReceiveSettings(const std::string& inAction, const std::string& inContext, const json& inPayload, const std::string& inDeviceID)
{
    mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveSettings()");
    if (mGlobalSettings->printDebug)
    {
        mConnectionManager->LogMessage("void StreamDeckMidiButton::DidReceiveSettings(): StoreButtonSettings() for button: " + inContext + " with settings: " + inPayload.dump().c_str());
    }
    StoreButtonSettings(inAction, inContext, inPayload, inDeviceID);
}

void StreamDeckMidiButton::SendNoteOn(const int midiChannel, const int midiNote, const int midiVelocity, const int noteOffMode)
{
    if (!mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::SendNoteOn()");
    else if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::SendNoteOn(Channel: " + std::to_string(midiChannel) + ", Note: " + std::to_string(midiNote) + ", Velocity: " + std::to_string(midiVelocity) + ", Note Off Mode: " + std::to_string(noteOffMode) + ")");

    //Note On message
    std::vector<unsigned char> midiMessage;
    midiMessage.push_back(143 + midiChannel);
    midiMessage.push_back(midiNote);
    midiMessage.push_back(midiVelocity);
    midiOut->sendMessage(&midiMessage);

    switch (noteOffMode)
    {
        case 0:
        {//no note off message
            if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::SendNoteOn(noteOffMode = 0): no note off message");
            break;
        }
        case PUSH_NOTE_OFF:
        {//1: {//send note off message immediately
            if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::SendNoteOn(noteOffMode = 1): send note off immediately");
            SendNoteOff(midiChannel, midiNote, midiVelocity);
            break;
        }
        case RELEASE_NOTE_OFF:
        {//2: {//send note off on KeyUp
            if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::SendNoteOn(noteOffMode = 2): send note off on KeyUp");
            break;
        }
        default:
        {
            break;
        }
    }
}

void StreamDeckMidiButton::SendNoteOff(const int midiChannel, const int midiNote, const int midiVelocity)
{
    mConnectionManager->LogMessage("void StreamDeckMidiButton::SendNoteOff()");
    
    //Note Off message
    std::vector<unsigned char> midiMessage;
    midiMessage.push_back(127 + midiChannel);
    midiMessage.push_back(midiNote);
    midiMessage.push_back(midiVelocity);
    midiOut->sendMessage(&midiMessage);
}

void StreamDeckMidiButton::SendCC(const int midiChannel, const int midiCC, const int midiValue)
{
    if (!mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::SendCC()");
    else if (mGlobalSettings->printDebug) mConnectionManager->LogMessage("void StreamDeckMidiButton::SendCC(Channel: " + std::to_string(midiChannel) + ", CC: " + std::to_string(midiCC) + ", Value: " + std::to_string(midiValue) + ")");
    
    //midi CC message
    std::vector<unsigned char> midiMessage;
    midiMessage.push_back(175 + midiChannel);
    midiMessage.push_back(midiCC);
    midiMessage.push_back(midiValue);
    midiOut->sendMessage(&midiMessage);
}

void StreamDeckMidiButton::SendMMC(const int sendMMC)
{
    mConnectionManager->LogMessage("void StreamDeckMidiButton::SendMMC()");
    
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
    mConnectionManager->LogMessage("void StreamDeckMidiButton::SendProgramChange()");
    
    //midi CC message
    std::vector<unsigned char> midiMessage;
    midiMessage.push_back(191 + midiChannel);
    midiMessage.push_back(midiProgramChange);
    midiOut->sendMessage(&midiMessage);

}

void StreamDeckMidiButton::SetActionIcon(const std::string& inAction, const std::string& inContext, const json &inPayload, const std::string& inDeviceID)
{
    mConnectionManager->LogMessage("void StreamDeckMidiButton::SetActionIcon()");
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
        mConnectionManager->LogMessage("void StreamDeckMidiButton::SetActionIcon(): something went wrong setting the action icon - the file is probably missing/unreadable");
    }
}

std::map <std::string, std::string> StreamDeckMidiButton::GetMidiPortList(Direction direction)
{
    switch (direction)
    {
        case Direction::OUT:
        {
            mConnectionManager->LogMessage("std::map <std::string, std::string> StreamDeckMidiButton::GetMidiPortList(Direction OUT)");
            std::map <std::string, std::string> midiPortList;
            std::string portName;
            unsigned int nPorts = midiOut->getPortCount();
            if (nPorts == 0)
            {
                midiPortList.insert(std::make_pair("std::map <std::string, std::string> StreamDeckMidiButton::GetMidiPortList(Direction OUT): no ports available - this is an error", std::to_string(0)));
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
                    mConnectionManager->LogMessage("std::map <std::string, std::string> StreamDeckMidiButton::GetMidiPortList(Direction OUT): " + it->first + " has port index: " + it->second);
                    it++;
                }
            }
            return midiPortList;
        }
        case Direction::IN:
        {
            mConnectionManager->LogMessage("std::map <std::string, std::string> StreamDeckMidiButton::GetMidiPortList(Direction IN)");
            std::map <std::string, std::string> midiPortList;
            std::string portName;
            unsigned int nPorts = midiIn->getPortCount();
            if (nPorts == 0)
            {
                midiPortList.insert(std::make_pair("std::map <std::string, std::string> StreamDeckMidiButton::GetMidiPortList(Direction IN): no ports available - this is an error", std::to_string(0)));
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
                    mConnectionManager->LogMessage("std::map <std::string, std::string> StreamDeckMidiButton::GetMidiPortList(Direction IN): " + it->first + " has port index: " + it->second);
                    it++;
                }
            }
            return midiPortList;
        }
    }
}

std::string StreamDeckMidiButton::GetPngAsString(const char* filename)
{
    mConnectionManager->LogMessage("std::string StreamDeckMidiButton::GetPngAsString()");
    //try reading the file
    std::vector<unsigned char> imageFile = ReadFile(filename);
    if (imageFile.empty())
    {
        mConnectionManager->LogMessage("std::string StreamDeckMidiButton::GetPngAsString(): empty image file");
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
    mConnectionManager->LogMessage("std::vector<unsigned char> StreamDeckMidiButton::ReadFile()");
    // open the file:
    std::ifstream file(filename, std::ios::binary);
    std::vector<unsigned char> vec;
    
    if (!filename)
    {
        mConnectionManager->LogMessage("std::vector<unsigned char> StreamDeckMidiButton::ReadFile(): couldn't open file " + std::string(filename));
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

StreamDeckMidiButton::FadeSet::FadeSet ()
{

}

StreamDeckMidiButton::FadeSet::FadeSet (const int fromValue, const int toValue, const float fadeTime, const float fadeCurve, const int sampleInterval)
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
        if (this->currentDirection == Direction::IN)
        {
            this->currentIndex = this->setSize;
            this->reverseFade = true;
            this->fadeActive = true;
        }
        else if (this->currentDirection == Direction::OUT)
        {
            this->currentIndex = this->setSize;
            this->reverseFade = true;
            this->fadeActive = true;
        }
    }
    else if (this->fadeActive)
    {
        this->reverseFade = true;
    }
}

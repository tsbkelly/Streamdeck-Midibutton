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

namespace {
const char* SEND_NOTE_ON = "uk.co.clarionmusic.midibutton.noteon";
const char* SEND_CC = "uk.co.clarionmusic.midibutton.cc";
const char* SEND_MMC = "uk.co.clarionmusic.midibutton.mmc";
const char* SEND_PROGRAM_CHANGE = "uk.co.clarionmusic.midibutton.programchange";
const int PUSH_NOTE_OFF = 1;
const int RELEASE_NOTE_OFF = 2;
}// namespace

//using this for debugging
inline std::string const BoolToString(bool b)
{
  return b ? "true" : "false";
}

StreamDeckMidiButton::StreamDeckMidiButton()
{
    //check whether we have a stored port name
    std::string portName;
    if (mGlobalSettings.portName != DEFAULT_PORT_NAME)
    {
        //we have a stored portName already
        portName = mGlobalSettings.portName;
    }
    else portName = DEFAULT_PORT_NAME;
    //RtMidiOut constructor
    try
    {
        midiOut = new RtMidiOut();
        midiIn = new RtMidiIn();
        
        if (mGlobalSettings.useVirtualPort)
        {
            try
            {
                midiOut->openVirtualPort(portName);
                midiIn->openVirtualPort(portName);
            }
            catch (RtMidiError &error)
            {
                if (mGlobalSettings.printDebug) mConnectionManager->LogMessage(error.getMessage());
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            try
            {
                midiOut->openPort(mGlobalSettings.selectedOutPortIndex);
                midiIn->openPort(mGlobalSettings.selectedInPortIndex);
            }
            catch (RtMidiError &error)
            {
                if (mGlobalSettings.printDebug) mConnectionManager->LogMessage(error.getMessage());
                exit(EXIT_FAILURE);
            }
        }
    }
    catch (RtMidiError &error)
    {
        if (mGlobalSettings.printDebug) mConnectionManager->LogMessage(error.getMessage());
        exit(EXIT_FAILURE);
    }
}

StreamDeckMidiButton::~StreamDeckMidiButton()
{
    if (midiOut != nullptr)
    {
        delete midiOut;
    }
    if (midiIn != nullptr)
    {
        delete midiIn;
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
        if (mGlobalSettings.printDebug)mConnectionManager->LogMessage("Setting midiChannel to " + std::to_string(thisButtonSettings.midiChannel));
    }
    if (inPayload["settings"].find("midiNote") != inPayload["settings"].end())
    {
        thisButtonSettings.midiNote = inPayload["settings"]["midiNote"];
        if (mGlobalSettings.printDebug)mConnectionManager->LogMessage("Setting midiNote to " + std::to_string(thisButtonSettings.midiNote));
    }
    if (inPayload["settings"].find("midiVelocity") != inPayload["settings"].end())
    {
        thisButtonSettings.midiVelocity = inPayload["settings"]["midiVelocity"];
        if (mGlobalSettings.printDebug)mConnectionManager->LogMessage("Setting midiVelocity to " + std::to_string(thisButtonSettings.midiVelocity));
    }
    if (inPayload["settings"].find("noteOffParams") != inPayload["settings"].end())
    {
        std::string mNO = inPayload["settings"]["noteOffParams"];
        std::stringstream MNO(mNO);
        int noteOffParams = 0;
        MNO >> noteOffParams;
        thisButtonSettings.noteOffParams = noteOffParams;
        if (mGlobalSettings.printDebug)mConnectionManager->LogMessage("Setting noteOffParams to " + std::to_string(thisButtonSettings.noteOffParams));
    }
    if (inPayload["settings"].find("midiProgramChange") != inPayload["settings"].end())
    {
        thisButtonSettings.midiProgramChange = inPayload["settings"]["midiProgramChange"];
        if (mGlobalSettings.printDebug)mConnectionManager->LogMessage("Setting midiProgramChange to " + std::to_string(thisButtonSettings.midiProgramChange));
    }
    if (inPayload["settings"].find("midiCC") != inPayload["settings"].end())
    {
        thisButtonSettings.midiCC = inPayload["settings"]["midiCC"];
        if (mGlobalSettings.printDebug)mConnectionManager->LogMessage("Setting midiCC to " + std::to_string(thisButtonSettings.midiCC));
    }
    if (inPayload["settings"].find("midiValue") != inPayload["settings"].end())
    {
        thisButtonSettings.midiValue = inPayload["settings"]["midiValue"];
        if (mGlobalSettings.printDebug)mConnectionManager->LogMessage("Setting midiValue to " + std::to_string(thisButtonSettings.midiValue));
    }
    if (inPayload["settings"].find("midiMMC") != inPayload["settings"].end())
    {
        thisButtonSettings.midiMMC = inPayload["settings"]["midiMMC"];
        if (mGlobalSettings.printDebug)mConnectionManager->LogMessage("Setting midiMMC to " + std::to_string(thisButtonSettings.midiMMC));
    }
    
    //store everything into the map
    if (storedButtonSettings.insert(std::make_pair(inContext, thisButtonSettings)).second == false)
    {
        //key already exists, so replace it
        storedButtonSettings[inContext] = thisButtonSettings;
    }
    
    //if it's an MMC button set the icon
    if (inAction == SEND_MMC) SetActionIcon(inAction, inContext, inPayload, inDeviceID);
    
    if (mGlobalSettings.printDebug)//check it's been stored correctly
    {
        mConnectionManager->LogMessage("midiChannel for " + inContext + "is set to: " + std::to_string(storedButtonSettings[inContext].midiChannel));
        mConnectionManager->LogMessage("midiNote for " + inContext + "is set to: " + std::to_string(storedButtonSettings[inContext].midiNote));
        mConnectionManager->LogMessage("midiVelocity for " + inContext + "is set to: " + std::to_string(storedButtonSettings[inContext].midiVelocity));
        mConnectionManager->LogMessage("noteOffParams for " + inContext + "is set to: " + std::to_string(storedButtonSettings[inContext].noteOffParams));
        mConnectionManager->LogMessage("midiProgramChange for " + inContext + "is set to: " + std::to_string(storedButtonSettings[inContext].midiProgramChange));
        mConnectionManager->LogMessage("midiCC for " + inContext + "is set to: " + std::to_string(storedButtonSettings[inContext].midiCC));
        mConnectionManager->LogMessage("midiValue for " + inContext + "is set to: " + std::to_string(storedButtonSettings[inContext].midiValue));
        mConnectionManager->LogMessage("midiMMC for " + inContext + "is set to: " + std::to_string(storedButtonSettings[inContext].midiMMC));
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
                if (storedButtonSettings[inContext].noteOnOffToggle)
                {
                    if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("noteOnOffToggle is set to " + BoolToString(storedButtonSettings[inContext].noteOnOffToggle));
                    SendNoteOn(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiNote, storedButtonSettings[inContext].midiVelocity, storedButtonSettings[inContext].noteOffParams);
                    storedButtonSettings[inContext].noteOnOffToggle = !storedButtonSettings[inContext].noteOnOffToggle;
                }
                else if (!storedButtonSettings[inContext].noteOnOffToggle)
                {
                    if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("noteOnOffToggle is set to " + BoolToString(storedButtonSettings[inContext].noteOnOffToggle));
                    SendNoteOff(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiNote, storedButtonSettings[inContext].midiVelocity);
                    storedButtonSettings[inContext].noteOnOffToggle = !storedButtonSettings[inContext].noteOnOffToggle;
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
            
            /*if (inPayload["settings"].find("midiChannel") == inPayload["settings"].end())
            {
                 if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("midiChannel not set");
                 throw;
            }
            int midiChannel = inPayload["settings"]["midiChannel"];
            
            if (inPayload["settings"].find("midiCC") == inPayload["settings"].end())
            {
                 if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("midiCC not set");
                 throw;
            }
            int midiCC = inPayload["settings"]["midiCC"];

            if (inPayload["settings"].find("midiValue") == inPayload["settings"].end())
            {
                 if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("midiValue not set");
                 throw;
            }
            int midiValue = inPayload["settings"]["midiValue"];*/
            
            SendCC(storedButtonSettings[inContext].midiChannel, storedButtonSettings[inContext].midiCC, storedButtonSettings[inContext].midiValue);
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
            
            /*std::string mNO = inPayload["settings"]["noteOffParams"];
            std::stringstream MNO(mNO);
            int noteOffParams = 0;
            MNO >> noteOffParams;*/
            
            if (storedButtonSettings[inContext].noteOffParams == 2)//(noteOffParams == 2)
            {
                /*std::string mC = inPayload["settings"]["midiChannel"];
                std::stringstream MC(mC);
                int midiChannel = 0;
                MC >> midiChannel;*/
                //int midiChannel = inPayload["settings"]["midiChannel"];
                
                /*std::string mN = inPayload["settings"]["midiNote"];
                std::stringstream MN(mN);
                int midiNote = 0;
                MN >> midiNote;*/
                //int midiNote = inPayload["settings"]["midiNote"];

                /*std::string mV = inPayload["settings"]["midiVelocity"];
                std::stringstream MV(mV);
                int midiVelocity = 0;
                MV >> midiVelocity;*/
                //int midiVelocity = inPayload["settings"]["midiVelocity"];

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
    if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("WillAppearForAction() - setting the storedButtonSettings for button: " + inContext);
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
	// Nothing to do
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
        return;
    }
    else
    {
        const auto event = EPLJSONUtils::GetStringByName(inPayload, "event");
        if (event == "getMidiPorts")
        {
            if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("Trying to get list of Output MIDI ports");
            std::map <std::string, std::string> midiPortList = GetMidiPortList(Direction::OUTPUT);
            mConnectionManager->SendToPropertyInspector(inAction, inContext,json({{"event", "midiOutPorts"},{"midiOutPortList", midiPortList}}));
            if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("Trying to get list of Input MIDI ports");
            midiPortList = GetMidiPortList(Direction::INPUT);
            mConnectionManager->SendToPropertyInspector(inAction, inContext,json({{"event", "midiInPorts"},{"midiInPortList", midiPortList}}));
            return;

        }
        else
        {
            //something's gone wrong
            if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("something's gone wrong - not expecting this message to be sent to SendToPlugin()");
        }
    }
}

void StreamDeckMidiButton::DidReceiveGlobalSettings(const json& inPayload)
{
    //check to see whether the DEBUG flag is set
    if (inPayload["settings"].find("printDebug") != inPayload["settings"].end()) mGlobalSettings.printDebug = inPayload["settings"]["printDebug"];
    if (inPayload["settings"].find("useVirtualPort") != inPayload["settings"].end()) mGlobalSettings.useVirtualPort = inPayload["settings"]["useVirtualPort"];
    
    mConnectionManager->LogMessage("mGlobalSettings.printDebug is set to " + BoolToString(mGlobalSettings.printDebug));
    if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("Successfully set the printDebug flag");

    //need to check that the midi port is a valid name - no special characters etc.
    //first, check that the portName actually has changed, to avoid unnecessarily re-opening the port
    //TODO
    std::string portName = inPayload["settings"]["portName"];
    
    /*if (inPayload["settings"]["portName"] == mGlobalSettings.portName && mGlobalSettings.useVirtualPort)
    {
        // don't need to do anything, as the portName hasn't changed
        return;
    }*/
    
    mGlobalSettings.portName = inPayload["settings"]["portName"];
    mGlobalSettings.selectedOutPortIndex = inPayload["settings"]["selectedOutPortIndex"];
    mGlobalSettings.selectedInPortIndex = inPayload["settings"]["selectedInPortIndex"];
    
    if (mGlobalSettings.printDebug)
    {
        mConnectionManager->LogMessage("Selected OUTPUT port index is " + std::to_string(mGlobalSettings.selectedOutPortIndex) + " & selected port name is " + midiOut->getPortName(mGlobalSettings.selectedOutPortIndex));
        mConnectionManager->LogMessage("Selected INPUT port index is " + std::to_string(mGlobalSettings.selectedInPortIndex) + " & selected port name is " + midiOut->getPortName(mGlobalSettings.selectedInPortIndex));
    }
    
    //reinitialise the midi port here with the correct name
    if (midiOut != nullptr)
    {
        delete midiOut;
        try
        {
            midiOut = new RtMidiOut();
            midiIn = new RtMidiIn();
            if (mGlobalSettings.useVirtualPort)
            {
                if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("opening virtual OUTPUT port called: " + midiOut->getPortName(mGlobalSettings.selectedOutPortIndex));
                midiOut->openVirtualPort(mGlobalSettings.portName);
                if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("opening virtual INPUT port called: " + midiOut->getPortName(mGlobalSettings.selectedOutPortIndex));
                midiIn->openVirtualPort(mGlobalSettings.portName);
            }
            else
            {
                // open the selected OUTPUT port
                if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("opening OUTPUT port index: " + std::to_string(mGlobalSettings.selectedOutPortIndex) + " with name: " + midiOut->getPortName(mGlobalSettings.selectedOutPortIndex));
                midiOut->closePort();
                midiOut->openPort(mGlobalSettings.selectedOutPortIndex);
                
                // open the selected INPUT port
                if (mGlobalSettings.printDebug) mConnectionManager->LogMessage("opening INPUT port index: " + std::to_string(mGlobalSettings.selectedInPortIndex) + " with name: " + midiOut->getPortName(mGlobalSettings.selectedInPortIndex));
                midiIn->closePort();
                midiIn->openPort(mGlobalSettings.selectedInPortIndex);
            }
        }
        catch (RtMidiError &error)
        {
            if (mGlobalSettings.printDebug) mConnectionManager->LogMessage(error.getMessage());
            exit(EXIT_FAILURE);
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
                    mConnectionManager->LogMessage(it->first);
                    mConnectionManager->LogMessage(it->second);
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
                    mConnectionManager->LogMessage(it->first);
                    mConnectionManager->LogMessage(it->second);
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
    std::vector<unsigned char> imageFile = readFile(filename);
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

std::vector<unsigned char> StreamDeckMidiButton::readFile(const char* filename)
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

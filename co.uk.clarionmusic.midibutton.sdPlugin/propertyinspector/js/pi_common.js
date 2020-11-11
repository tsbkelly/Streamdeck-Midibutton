function didReceiveGlobalSettings(jsonObj)
{
    settings = jsonObj.payload.settings;
    document.getElementById("printDebug").checked = settings.printDebug;
    document.getElementById("portName").value = settings.portName;
    $SD.api.sendToPlugin(uuid, actionInfo, {event: "getMidiPorts"});
    document.getElementById("outPortList").style.display = document.getElementById("useVirtualPort").checked ? "none" : "";
    document.getElementById("inPortList").style.display = document.getElementById("useVirtualPort").checked ? "none" : "";
}
        
function receivedDataFromPlugin(jsonObj)
{
    const payload = jsonObj["payload"];
    if (payload["event"] == "midiOutPorts")
    {
        const outPortSelector = document.getElementById("outPortListSelector");
        while (outPortSelector.firstChild)
        {
            outPortSelector.removeChild(outPortSelector.firstChild);
        }
        var count = 0;
        for (i in payload["midiOutPortList"])
        {
            const outPortOption = document.createElement("option");
            outPortOption.text = i;
            //console.log("Midi OUT port with name " + i + " has index: " + payload["midiOutPortList"][i]);
            outPortOption.value = payload["midiOutPortList"][i];
            outPortSelector.appendChild(outPortOption);
            count++;
        }
    }
    else if (payload["event"] == "midiOutPortSelected")
    {
        document.getElementById("outPortListSelector").value = payload.midiOutPortSelected;
    }
    else if (payload["event"] == "midiInPorts")
    {
        const inPortSelector = document.getElementById("inPortListSelector");
        while (inPortSelector.firstChild)
        {
            inPortSelector.removeChild(inPortSelector.firstChild);
        }

        var count = 0;
        for (i in payload["midiInPortList"])
        {
            const inPortOption = document.createElement("option");
            inPortOption.text = i;
            //console.log("Midi IN port with name " + i + " has index: " + payload["midiInPortList"][i]);
            inPortOption.value = payload["midiInPortList"][i];
            inPortSelector.appendChild(inPortOption);
            count++;
        }
    }
    else if (payload["event"] == "midiInPortSelected")
    {
        document.getElementById("inPortListSelector").value = payload.midiInPortSelected;
    }
    else return;
}
                                                                     
function saveSettings()
{
    //clear the current settings
    for (var key in settings) delete settings[key];
    //save settings here
    if (actionInfo == "uk.co.clarionmusic.midibutton.noteon")
    {
        settings.midiChannel = +document.getElementById("midiChannel").value;
        settings.midiNote = +document.getElementById("midiNote").value;
        settings.midiVelocity = +document.getElementById("midiVelocity").value;
        settings.noteOffParams = document.getElementById("noteOffParams").value;
    }
    if (actionInfo == "uk.co.clarionmusic.midibutton.programchange")
    {
        settings.midiChannel = +document.getElementById("midiChannel").value;
        settings.midiProgramChange = +document.getElementById("midiProgramChange").value;
    }
    if (actionInfo == "uk.co.clarionmusic.midibutton.cc")
    {
        settings.midiChannel = +document.getElementById("midiChannel").value;
        settings.midiCC = +document.getElementById("midiCC").value;
        settings.midiValue = +document.getElementById("midiValue").value;
        settings.midiValueSec = +document.getElementById("midiValueSec").value;
        settings.toggleCC = document.getElementById("toggleCC").checked;
        document.getElementById("midiValueSecondary").style.display = document.getElementById("toggleCC").checked ? "" : "none";
    }
    if (actionInfo == "uk.co.clarionmusic.midibutton.cctoggle")
    {
        console.log("got here");
        settings.midiChannel = +document.getElementById("midiChannel").value;
        settings.midiCC = +document.getElementById("midiCC").value;
        settings.midiValue = +document.getElementById("midiValue").value;
        settings.midiValueSec = +document.getElementById("midiValueSec").value;
    }
    if (actionInfo == "uk.co.clarionmusic.midibutton.mmc")
    {
        settings.midiMMC = +document.getElementById("midiMMC").value;
        //set the icon of the action to the correct image
        $SD.api.sendToPlugin(uuid, actionInfo, {midiMMC: settings.midiMMC});
    }
    $SD.api.setSettings(uuid, settings);
}

function saveGlobalSettings()
{
    //clear the current settings
    for (var key in settings) delete settings[key];
                                                                     
    //save settings here
    settings.printDebug = document.getElementById("printDebug").checked;
    settings.portName = document.getElementById("portName").value;
    settings.selectedOutPortIndex = +document.getElementById("outPortListSelector").value;
    settings.selectedInPortIndex = +document.getElementById("inPortListSelector").value;
    $SD.api.setGlobalSettings(uuid, settings);
    $SD.api.sendToPlugin(uuid, actionInfo, {event: "getMidiPorts"});
    document.getElementById("outPortList").style.display = document.getElementById("useVirtualPort").checked ? "none" : "";
    document.getElementById("inPortList").style.display = document.getElementById("useVirtualPort").checked ? "none" : "";
}

function getMidiNote(note)
{
    if ((typeof note === 'number' || typeof note === 'string') && note > 0 && note < 128) return +note;
    var p = Array.isArray(note) ? note : parse(note);
    if (!p || p.length < 2) return null;
    return p[0] * 7 + p[1] * 12 + 12;
}

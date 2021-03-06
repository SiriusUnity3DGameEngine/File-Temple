// Copyright 2018 E*D Films. All Rights Reserved.

/**
 * midiWriter.cpp
 *
 * Writer interfaces to turn an EventGraph into Midi or XML files
 * 
 * @author  dotBunny <hello@dotbunny.com>
 * @version 1
 * @since	  1.0.0
 */

#include "midiWriter.h"
#include "midiMidiFile.h"

#include <fstream>
#include <sstream>
#include <iomanip> 

namespace SceneTrackMidi
{

  void EventXmlWriter(const char* dst, EventGraph* graph, std::map<u32, std::string>& names)
  {
    std::ofstream file;
    file.open(dst);
    
    file << std::setprecision(9);
    file << "<?xml version=\"1.0\" encoding=\"ASCII\" standalone=\"yes\"?>" << std::endl;

    for(auto track : graph->tracks)
    {
      file << "<object id=\"" << track.second->objectId << "\"";
      
      u64 evtCount = track.second->events.size();
      const auto trackName = names.find(track.second->objectId);

      if (trackName != names.end())
      { 
        file << " name=\"" << trackName->second << "\"";
      }

      file << "event-count=\"" << evtCount << "\"";

      file << ">" << std::endl;
       
      if (track.second->userData.length() > 0)
      {
        file << "\t<userdata>" << std::endl;
        file << "\t<![CDATA[";
        file << track.second->userData;
        file << "]]>" << std::endl;
        file << "\t</userdata>" << std::endl;
      }
      
      u64 evtId = 0;
      
      u64 time_sum_us = 0;
      f64 time = 0;

      for(auto& evt : track.second->events)
      {
        time = evt.frameLength * 1000000.0;
        time_sum_us += (u64) time;
        evtId++;
        switch(evt.klass)
        {
          case EventClass::PhysicsEvent: 
            file << "\t<physics-event ";
          break;
        }

        file << "frame-number=\"" << evt.frameNumber << "\" frame-time=\"" << evt.frameTime << "\"";
        file << " frame-length=\"" << evt.frameLength << "\"";
        file << " event-number=\"" << evtId << "\"";
        file << " calc-time-us=\"" << time_sum_us << "\"";
        file << ">" << std::endl;
        
        file << "\t<other";

        file << " id=\"" << evt.otherObject << "\"";

        const auto otherName = names.find(evt.otherObject);
        
        if (otherName != names.end())
        { 
          file << " name=\"" << trackName->second << "\"";
        }

        file << "/>";
        
        file << "\t<event>";
        switch(evt.event)
        {
          case EventType::Start:    file << "Start";    break;
          case EventType::Stop:     file << "Stop";     break;
          case EventType::Continue: file << "Continue"; break;
        }
        file << "</event>" << std::endl;
        
        file << "\t<position>" << std::endl;
        file << "\t\t<x>" << evt.worldPosition.x << "</x>" << std::endl;
        file << "\t\t<y>" << evt.worldPosition.y << "</y>" << std::endl;
        file << "\t\t<z>" << evt.worldPosition.z << "</z>" << std::endl;
        file << "\t</position>" << std::endl;

        switch(evt.klass)
        {
          case EventClass::PhysicsEvent: 
          file << "\t</physics-event>" << std::endl;
          break;
        }
      }

      file << "</object>" << std::endl;

    }

  }

  u8 FindNote(u32* notes, u32 object)
  {
    for(u32 i=0;i < 127;i++)
    {
      if (notes[i] == object)
        return (u8) i;
    }
    return 0x7F;
  }

  u8 AddNote(u32* notes, u32 object)
  {
    for(u32 i=0;i < 127;i++)
    {
      if (notes[i] == 0)
      {
        notes[i] = object;
        return (u8) i;
      }
    }
    return 0x7F;
  }

  void RemoveNote(u32* notes, u32 object)
  {
    for(u32 i=0;i < 127;i++)
    {
      if (notes[i] == object)
      {
        notes[i] = 0;
        return;
      }
    }
  }

  void ResetSs(std::stringstream& ss)
  {
    ss.str("");
    ss.clear();
  }

  u8 NormaliseNote(u8 note)
  {
    u8 n = note + 60; // C4

    if (n >= 0x7F)
    {
      n = 60 - note;
    }

    return n;
  }

  void EventMidiWriter(const char* dst, EventGraph* graph, std::map<u32, std::string>& names)
  {
    MidiFile file;

    u32 notes[127];
    for(u32 i=0;i < 127;i++)
      notes[i] = 0;
    std::stringstream ss;

    file.Open(dst, (u16) graph->tracks.size());
    f64 time = 0.0;

    for(const auto& trackKv : graph->tracks)
    {
      for(u32 i=0;i < 127;i++)
        notes[i] = 0;

      auto track = trackKv.second;

      ResetSs(ss);

      const auto& trackName = names.find(track->objectId);

      if (trackName != names.end())
      { 
        ss << trackName->second;
      }
      else
      {
        ss << "Object_" << track->objectId;
      }

      std::string objectName = ss.str();

      file.StartTrack(objectName);

      if (track->userData.length() > 0)
      {
        file.WriteLyric(0, 0, track->userData);
      }

      u64 evtCount = track->events.size();
      u64 evtId = 0;
      u64 time_sum_us = 0;
      for(const auto& evt : track->events)
      {
        evtId++;
        time = evt.frameTime * 1000000.0;
        time_sum_us = (u64) time;

        if (evt.event == EventType::Start)
        {
          f32 velValue = evt.strength;
          
          // Normalise float (-1 to +1) to 7-bit.  (Range is essentially 1=-63 to 127=63)
          if (velValue < -1.0f)
            velValue = -1.0f;
          else if (velValue > 1.0f)
            velValue = 1.0f;

          u8 velocity = 64 + (u8) ((velValue * 63.0f));

          u8 note = FindNote(notes, evt.otherObject);

          if (note == 0x7F)
          {
            note = AddNote(notes, evt.otherObject);
          }

          const auto otherName = names.find(evt.otherObject);

          ResetSs(ss);

          ss << time_sum_us << "," << evtId << ":" << evtCount << ",start," << objectName << ',';

          if (otherName != names.end())
          { 
            ss << otherName->second;
          }
          else
          {
            ss << "Obj" << track->objectId;
          }

          ss << ',' << evt.strength <<  ',' << evt.worldPosition.x << ',' << evt.worldPosition.y << ',' << evt.worldPosition.z;

          file.WriteCuePoint(time_sum_us, -1, ss.str());

          file.WriteOnNote(time_sum_us, NormaliseNote(note), velocity);
        }
        else if (evt.event == EventType::Stop)
        {
          u8 note = FindNote(notes, evt.otherObject);
          
          const auto otherName = names.find(evt.otherObject);

          ResetSs(ss);

          ss << time_sum_us << "," << evtId << ":" << evtCount << ",stop," << objectName << ',';

          if (otherName != names.end())
          { 
            ss << otherName->second;
          }
          else
          {
            ss << "Obj" << track->objectId;
          }

          ss << ',' << evt.strength <<  ',' << evt.worldPosition.x << ',' << evt.worldPosition.y << ',' << evt.worldPosition.z;

          file.WriteCuePoint(time_sum_us, -1, ss.str());

          file.WriteOffNote(time_sum_us, NormaliseNote(note));
          RemoveNote(notes, note);
        }
        
      }

      file.EndTrack();
    }


    file.Close();
  }

}

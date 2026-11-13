#pragma once
namespace dwg2::input {inline bool FullscreenEdge(float level,bool& latched){bool edge=level>=.65f&&!latched;if(level<=.25f)latched=false;else if(level>=.65f)latched=true;return edge;}}

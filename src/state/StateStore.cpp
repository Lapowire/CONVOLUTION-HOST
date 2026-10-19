#include "StateStore.h"
#include <juce_cryptography/juce_cryptography.h>
#include <cmath>
namespace ch {
juce::ValueTree StateStore::encode(const SavedState& s) {
 juce::ValueTree root("CONVOLUTION_HOST");root.setProperty("version",1,nullptr);
 root.addChild(s.parameters.createCopy(),-1,nullptr);
 juce::ValueTree ir("IR");ir.setProperty("path",s.request.file.getFullPathName(),nullptr);ir.setProperty("hash",s.hash,nullptr);ir.setProperty("relative",s.relativePath,nullptr);
 ir.setProperty("start",s.request.startMs,nullptr);ir.setProperty("end",s.request.endMs,nullptr);ir.setProperty("fadeIn",s.request.fadeInMs,nullptr);ir.setProperty("fadeOut",s.request.fadeOutMs,nullptr);ir.setProperty("normalise",s.request.normalise,nullptr);
 for(auto r:s.routing.routes){juce::ValueTree path("ROUTE");path.setProperty("input",r.input,nullptr);path.setProperty("output",r.output,nullptr);path.setProperty("irChannel",r.irChannel,nullptr);path.setProperty("gain",r.gain,nullptr);ir.addChild(path,-1,nullptr);}
 root.addChild(ir,-1,nullptr);return root;
}
bool StateStore::decode(const juce::ValueTree& root,SavedState& out) {
 if(!root.hasType("CONVOLUTION_HOST") || int(root["version"])!=1)return false;
 auto ir=root.getChildWithName("IR"),parameters=root.getChildWithName("PARAMETERS");if(!ir.isValid() || !parameters.isValid() || ir.getNumChildren()>256)return false;
 SavedState s;s.parameters=parameters.createCopy();const auto path=ir["path"].toString();
 if(path.isNotEmpty() && !juce::File::isAbsolutePath(path))return false;
 s.request.file=path.isEmpty()?juce::File{}:juce::File(path);s.hash=ir["hash"].toString();s.relativePath=ir["relative"].toString();
 if(juce::File::isAbsolutePath(s.relativePath) || s.relativePath.contains(".."))return false;
 s.request.startMs=double(ir["start"]);s.request.endMs=double(ir["end"]);s.request.fadeInMs=double(ir["fadeIn"]);s.request.fadeOutMs=double(ir["fadeOut"]);s.request.normalise=bool(ir["normalise"]);
 for(auto v:{s.request.startMs,s.request.endMs,s.request.fadeInMs,s.request.fadeOutMs})if(!std::isfinite(v)||v<0||v>30000)return false;
 for(auto child:ir){if(!child.hasType("ROUTE"))return false;Route r{int(child["input"]),int(child["output"]),int(child["irChannel"]),float(child["gain"])};
  if(r.input<0||r.input>15||r.output<0||r.output>15||r.irChannel<0||r.irChannel>15||!std::isfinite(r.gain)||std::abs(r.gain)>16)return false;s.routing.routes.push_back(r);}
 out=std::move(s);return true;
}
juce::File StateStore::resolve(const SavedState& s,const juce::StringArray& roots) {
 std::vector<juce::File> candidates;if(s.request.file!=juce::File{})candidates.push_back(s.request.file);
 for(auto root:roots){juce::File dir(root);const auto relative=s.relativePath.isNotEmpty()?s.relativePath:s.request.file.getFileName();if(relative.isNotEmpty())candidates.push_back(dir.getChildFile(relative));}
 for(const auto& file:candidates){if(!file.existsAsFile())continue;if(s.hash.isEmpty())return file;
  if(auto stream=file.createInputStream())if(juce::SHA256(*stream).toHexString()==s.hash)return file;}
 return {};
}
}

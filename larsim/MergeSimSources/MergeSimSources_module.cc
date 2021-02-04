////////////////////////////////////////////////////////////////////////
// Class:       MergeSimSources
// Module Type: producer
// File:        MergeSimSources_module.cc
//
// Generated at Tue Feb 17 12:16:35 2015 by Wesley Ketchum using artmod
// from cetpkgsupport v1_08_02.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "canvas/Utilities/InputTag.h"
#include "canvas/Utilities/Exception.h"
#include "fhiclcpp/types/Sequence.h"
#include "fhiclcpp/types/Atom.h"
#include "fhiclcpp/ParameterSet.h"

#include "art/Persistency/Common/PtrMaker.h"
#include "canvas/Persistency/Common/Assns.h"
#include "canvas/Persistency/Common/FindOneP.h"

#include <memory>
#include <vector>
#include <string>
#include <utility>

#include "lardataobj/Simulation/GeneratedParticleInfo.h"
#include "lardataobj/Simulation/SimPhotons.h"
#include "lardataobj/Simulation/SimChannel.h"
#include "nusimdata/SimulationBase/MCTruth.h"
#include "nusimdata/SimulationBase/MCParticle.h"
#include "larcorealg/CoreUtils/enumerate.h"
#include "larcorealg/CoreUtils/counter.h"
#include "larsim/Simulation/LArG4Parameters.h"
#include "MergeSimSources.h"

namespace sim {
  class MergeSimSources;
}

class sim::MergeSimSources : public art::EDProducer {
public:

  struct Config {

    fhicl::Sequence<art::InputTag> InputSourcesLabels {
      fhicl::Name{ "InputSourcesLabels" },
      fhicl::Comment{ "label of the LArG4 products to merge" }
      };

    fhicl::Sequence<int> TrackIDOffsets {
      fhicl::Name{ "TrackIDOffsets" },
      fhicl::Comment{ "offset to add to the MC particles from each source" }
      };

    fhicl::Atom<bool> StoreReflected {
      fhicl::Name{ "StoreReflected" },
      fhicl::Comment{ "whether to merge also photons from reflections" },
      false // default
      };

  }; // struct Config

  using Parameters = art::EDProducer::Table<Config>;

  explicit MergeSimSources(Parameters const & config);

  // Required functions.
  void produce(art::Event & e) override;

private:

  std::vector<art::InputTag> const fInputSourcesLabels;
  std::vector<int>           const fTrackIDOffsets;
  bool                       const fUseLitePhotons;
  bool                       const fStoreReflected;

  static std::string const ReflectedLabel;

  MergeSimSourcesUtility     fMergeUtility;

};


std::string const sim::MergeSimSources::ReflectedLabel { "Reflected" };

sim::MergeSimSources::MergeSimSources(Parameters const & params)
  : EDProducer{params}
  , fInputSourcesLabels(params().InputSourcesLabels())
  , fTrackIDOffsets(params().TrackIDOffsets())
  , fUseLitePhotons(art::ServiceHandle<sim::LArG4Parameters const>()->UseLitePhotons())
  , fStoreReflected(params().StoreReflected())
  , fMergeUtility(fTrackIDOffsets)
{

  if(fInputSourcesLabels.size() != fTrackIDOffsets.size()) {
    throw art::Exception(art::errors::Configuration)
      << "Unequal input vector sizes: InputSourcesLabels and TrackIDOffsets.\n";
  }


  for (art::InputTag const& tag: fInputSourcesLabels) {
    consumes<std::vector<simb::MCParticle>>(tag);
    consumes<art::Assns<simb::MCParticle, simb::MCTruth, sim::GeneratedParticleInfo>>(tag);
    consumes<std::vector<sim::SimChannel>>(tag);
    consumes<std::vector<sim::AuxDetSimChannel>>(tag);

    if(!fUseLitePhotons)
      consumes<std::vector<sim::SimPhotons>>(tag);
    else
      consumes<std::vector<sim::SimPhotonsLite>>(tag);

    if (fStoreReflected) {
      art::InputTag const reflected_tag { tag.label(), ReflectedLabel };
      if(!fUseLitePhotons)
        consumes<std::vector<sim::SimPhotons>>(reflected_tag);
      else
        consumes<std::vector<sim::SimPhotonsLite>>(reflected_tag);
    }

  } // for


  produces< std::vector<simb::MCParticle> >();
  produces< std::vector<sim::SimChannel>  >();
  produces< std::vector<sim::AuxDetSimChannel> >();
  produces< art::Assns<simb::MCTruth, simb::MCParticle, sim::GeneratedParticleInfo> >();
  
  if(!fUseLitePhotons) produces< std::vector<sim::SimPhotons>     >();
  else                 produces< std::vector<sim::SimPhotonsLite> >();

  if (fStoreReflected) {
    if(!fUseLitePhotons) produces< std::vector<sim::SimPhotons>     >(ReflectedLabel);
    else                 produces< std::vector<sim::SimPhotonsLite> >(ReflectedLabel);
  }

  
}

void sim::MergeSimSources::produce(art::Event & e)
{

  auto partCol = std::make_unique<std::vector<simb::MCParticle>>();
  auto scCol = std::make_unique<std::vector<sim::SimChannel>>();
  auto PhotonCol = std::make_unique<std::vector<sim::SimPhotons>>();
  auto LitePhotonCol = std::make_unique<std::vector<sim::SimPhotonsLite>>();
  auto ReflPhotonCol = std::make_unique<std::vector<sim::SimPhotons>>();
  auto ReflLitePhotonCol = std::make_unique<std::vector<sim::SimPhotonsLite>>();
  auto tpassn = std::make_unique<art::Assns<simb::MCTruth, simb::MCParticle, sim::GeneratedParticleInfo>>();
  auto adCol = std::make_unique<std::vector<sim::AuxDetSimChannel>>();

  fMergeUtility.Reset();

  art::PtrMaker<simb::MCParticle> const makePartPtr { e };

  for(auto const& [ i_source, input_label ]: util::enumerate(fInputSourcesLabels)) {

    auto const input_partCol
      = e.getValidHandle<std::vector<simb::MCParticle>>(input_label);
    fMergeUtility.MergeMCParticles(*partCol,*input_partCol,i_source);

    //truth-->particle assoc stuff here
    const std::vector<size_t>& assocVectorPrimitive
      = fMergeUtility.GetMCParticleListMap().at(i_source);
    art::FindOneP<simb::MCTruth, sim::GeneratedParticleInfo> mctAssn(input_partCol,e,input_label);
    for(auto const i_p: util::counter(mctAssn.size()))
      tpassn->addSingle(mctAssn.at(i_p), makePartPtr(assocVectorPrimitive[i_p]), mctAssn.data(i_p).ref());


    auto const& input_scCol
       = e.getByLabel<std::vector<sim::SimChannel>>(input_label);
    fMergeUtility.MergeSimChannels(*scCol,input_scCol,i_source);

    auto const& input_adCol
      = e.getByLabel<std::vector<sim::AuxDetSimChannel>>(input_label);
    fMergeUtility.MergeAuxDetSimChannels(*adCol,input_adCol,i_source);

    if(!fUseLitePhotons){
      auto const& input_PhotonCol
        = e.getByLabel<std::vector<sim::SimPhotons>>(input_label);
      fMergeUtility.MergeSimPhotons(*PhotonCol,input_PhotonCol);
    }
    else{
      auto const& input_LitePhotonCol
       = e.getByLabel<std::vector<sim::SimPhotonsLite>>(input_label);
      fMergeUtility.MergeSimPhotonsLite(*LitePhotonCol,input_LitePhotonCol);
    }

    if (fStoreReflected) {
      art::InputTag const input_reflected_label
        { input_reflected_label.label(), ReflectedLabel };
      if(!fUseLitePhotons){
        auto const& input_PhotonCol
          = e.getByLabel<std::vector<sim::SimPhotons>>(input_reflected_label);
        fMergeUtility.MergeSimPhotons(*ReflPhotonCol,input_PhotonCol);
      }
      else{
        auto const& input_LitePhotonCol
          = e.getByLabel<std::vector<sim::SimPhotonsLite>>(input_reflected_label);
        fMergeUtility.MergeSimPhotonsLite(*ReflLitePhotonCol,input_LitePhotonCol);
      }
    }

  }

  e.put(std::move(partCol));
  e.put(std::move(scCol));
  e.put(std::move(adCol));
  if(!fUseLitePhotons) e.put(std::move(PhotonCol));
  else                 e.put(std::move(LitePhotonCol));
  if(fStoreReflected) {
    if(!fUseLitePhotons) e.put(std::move(ReflPhotonCol), ReflectedLabel);
    else                 e.put(std::move(ReflLitePhotonCol), ReflectedLabel);
  }
  e.put(std::move(tpassn));

}

DEFINE_ART_MODULE(sim::MergeSimSources)

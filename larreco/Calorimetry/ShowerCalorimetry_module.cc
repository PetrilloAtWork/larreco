////////////////////////////////////////////////////////////////////////
// Class:       ShowerCalorimetry
// Plugin Type: producer (art v3_02_06)
// File:        ShowerCalorimetry_module.cc
//
// Generated at Fri Jul 12 14:14:46 2019 by Jacob Calcutt using cetskelgen
// from cetlib version v3_07_02.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "lardataobj/AnalysisBase/Calorimetry.h"
#include "lardataobj/RecoBase/SpacePoint.h"
#include "lardataobj/RecoBase/Shower.h"
#include "lardataobj/RecoBase/Hit.h"
#include "lardata/DetectorInfoServices/DetectorPropertiesService.h"

#include "canvas/Persistency/Common/FindManyP.h"
#include "lardataobj/AnalysisBase/Calorimetry.h"
#include "lardata/DetectorInfoServices/LArPropertiesService.h"
#include "lardata/Utilities/AssociationUtil.h"
#include "larreco/Calorimetry/CalorimetryAlg.h"

#include "larcore/Geometry/Geometry.h"

#include "larevt/SpaceCharge/SpaceCharge.h"
#include "larevt/SpaceChargeServices/SpaceChargeService.h"

#include <memory>
#include <TVector3.h>

namespace calo{
  class ShowerCalorimetry;
} 

class calo::ShowerCalorimetry : public art::EDProducer {
public:
  explicit ShowerCalorimetry(fhicl::ParameterSet const& p);
  // The compiler-generated destructor is fine for non-base
  // classes without bare pointers or other resource use.

  // Plugins should not be copied or assigned.
  ShowerCalorimetry(ShowerCalorimetry const&) = delete;
  ShowerCalorimetry(ShowerCalorimetry&&) = delete;
  ShowerCalorimetry& operator=(ShowerCalorimetry const&) = delete;
  ShowerCalorimetry& operator=(ShowerCalorimetry&&) = delete;

  // Required functions.
  void produce(art::Event& e) override;

  int GetShowerIndex( const recob::Shower & shower, art::Event const & evt ) const;

private:

  std::string fShowerTag;
  bool fSCE;
  CalorimetryAlg caloAlg;
};


calo::ShowerCalorimetry::ShowerCalorimetry(fhicl::ParameterSet const& p):
  EDProducer{p},
  fShowerTag( p.get< std::string >( "ShowerTag" ) ),
  fSCE(p.get< bool >("CorrectSCE")),
  caloAlg(p.get<fhicl::ParameterSet>("CalorimetryAlg"))
{
  produces< std::vector< anab::Calorimetry > >();
  produces< art::Assns< recob::Shower, anab::Calorimetry > >();
}

void calo::ShowerCalorimetry::produce(art::Event& e) {

  art::ServiceHandle< geo::Geometry > geom;

  auto const* detprop = lar::providerFrom<detinfo::DetectorPropertiesService>();
  auto const* sce = lar::providerFrom<spacecharge::SpaceChargeService>();


  //Make the container for the calo product to put onto the event.
  std::unique_ptr< std::vector<anab::Calorimetry> > caloPtr(new std::vector<anab::Calorimetry>);
  std::vector< anab::Calorimetry > & caloVector(*caloPtr);

  //Make a container for the track<-->calo associations.
  //One entry per track, with entry equal to index in calorimetry collection of associated object.
  std::vector< size_t > assnShowerCaloVector;
  std::unique_ptr< art::Assns< recob::Shower,anab::Calorimetry> > associationPtr( new  art::Assns< recob::Shower, anab::Calorimetry > );





  //Get the shower handle
  auto showerHandle = e.getValidHandle< std::vector< recob::Shower > >(fShowerTag);

  //Turn it into a vector of art pointers
  std::vector< art::Ptr< recob::Shower > > recoShowers;
  art::fill_ptr_vector( recoShowers, showerHandle );

  //Also get the hits from all the showers
  art::FindManyP<recob::Hit> findHitsFromShowers(showerHandle,e,fShowerTag);
  //Go through all of the reconstructed showers in the event
  for( size_t i = 0; i < recoShowers.size(); ++i ){
    const recob::Shower & shower = *(recoShowers.at(i));

    int shower_index = GetShowerIndex( shower, e );
    MF_LOG_INFO("ShowerCalorimetry") << "Getting Calorimetry info for " << shower_index << "\n";

    //This wil be used in the calorimetry object later
    float shower_length = shower.Length();
    //Get the hits from this shower 
    std::vector< art::Ptr< recob::Hit > > hits = findHitsFromShowers.at( shower_index );


    art::FindManyP<recob::SpacePoint> spFromShowerHits(hits,e,"hitpdune");
    std::cout<<"SP "<<spFromShowerHits.size()<<std::endl;
    
    //Sort the hits by their plane 
    //This vector stores the index of each hit on each plane 
    std::vector< std::vector< size_t > > hit_indices_per_plane( geom->Nplanes() );
    for( size_t j = 0; j < hits.size(); ++j ){
      hit_indices_per_plane[ hits[j]->WireID().Plane ].push_back( j );
    }

    //Go through each plane and make calorimetry objects
    for( size_t j = 0; j < geom->Nplanes(); ++j ){

      size_t hits_in_plane = hit_indices_per_plane[j].size();

      //Reserve vectors for each part of the calorimetry object
      std::vector< float > dEdx( hits_in_plane );
      std::vector< float > dQdx( hits_in_plane );
      std::vector< float > pitch( hits_in_plane );

      //residual range, xyz, and deadwire default for now
      std::vector< float > resRange( hits_in_plane, 0. );
      std::vector< TVector3 > xyz( hits_in_plane, TVector3(0.,0.,0.) );
      std::vector< float > deadwires( hits_in_plane, 0. );

      geo::PlaneID planeID( 0, 0, j );

      float kineticEnergy = 0.;

      for( size_t k = 0; k < hits_in_plane; ++k ){  
        size_t hit_index = hit_indices_per_plane[j][k];
        std::cout << "\tHit " << hit_index << std::endl;
        auto theHit = hits[ hit_index ];        
        float this_pitch = geom->WirePitch( planeID );
        float theHit_Xpos = detprop->ConvertTicksToX(theHit->PeakTime(),theHit->WireID().Plane,theHit->WireID().TPC,0);

        //Y and Z from SP ... to do fill out Y and Z
        TVector3 pos(theHit_Xpos,0,0);
        const double tmp_hit_pos[3]={pos.X(), pos.Y(), pos.Z()};
        geo::TPCID tpcid = geom->FindTPCAtPosition ( tmp_hit_pos  );

        //Correct for SCE
        geo::Vector_t posOffsets = {0., 0., 0.};
        geo::Vector_t dirOffsets = {0., 0., 0.};

        if( fSCE && sce->EnableCalSpatialSCE() )
          posOffsets = sce->GetCalPosOffsets(geo::Point_t(pos),tpcid.TPC);
          
        //For now, use the shower direction from Pandora...a better idea?
        if( fSCE && sce->EnableCalSpatialSCE() )
          dirOffsets = sce->GetCalPosOffsets(geo::Point_t{pos.X() + this_pitch*shower.Direction().X(), pos.Y() + this_pitch*shower.Direction().Y(), pos.Z() + this_pitch*shower.Direction().Z()},tpcid.TPC);
          
        TVector3 dir_corr = {this_pitch*shower.Direction().X() - dirOffsets.X() + posOffsets.X(), this_pitch*shower.Direction().Y() + dirOffsets.Y() - posOffsets.Y(), this_pitch*shower.Direction().Z() + dirOffsets.Z() - posOffsets.Z()};

         
        dQdx[k] = theHit->Integral() / this_pitch;
        //Just for now, use dQdx for dEdx
        //dEdx[k] = theHit->Integral() / this_pitch; 
        dEdx[k] = caloAlg.dEdx_AREA(*theHit,this_pitch),
        pitch[k] = this_pitch;

        kineticEnergy += dEdx[k];

      }
      
      //Make a calo object in the vector 
      caloVector.emplace_back(
        kineticEnergy,
        dEdx,
        dQdx,
        resRange,
        deadwires,
        shower_length,
        pitch,
        recob::tracking::convertCollToPoint(xyz),
        planeID
      );

      //Place the shower index in the association object
      assnShowerCaloVector.emplace_back( shower_index );
    }
    
  }

  //Make the associations for ART 
  for( size_t i = 0; i < assnShowerCaloVector.size(); i++ ){
    if( assnShowerCaloVector[i] == std::numeric_limits< size_t >::max() ) continue;

    art::Ptr<recob::Shower> shower_ptr(showerHandle,assnShowerCaloVector[i]);
    util::CreateAssn(*this, e, caloVector, shower_ptr, *associationPtr, i);
  }



  //Finish up: Put the objects into the event
  e.put( std::move( caloPtr ) );
  e.put( std::move( associationPtr ) );
  

}

int calo::ShowerCalorimetry::GetShowerIndex( const recob::Shower & shower, art::Event const & evt ) const{
  if(shower.ID() != -999) return shower.ID();

  auto recoShowers = evt.getValidHandle<std::vector<recob::Shower> >(fShowerTag);

  // Iterate through all showers to find the matching one to our shower
  int actualIndex = shower.ID();
  if(shower.ID() < 0){
    for(unsigned int s = 0; s < recoShowers->size(); ++s){
      const recob::Shower thisShower = (*recoShowers)[s];
      // Can't compare actual objects so look at a property
      if(fabs(thisShower.Length() - shower.Length()) < 1.e-5){
        actualIndex = s;
        continue;
      }
    }
  }

  return actualIndex;
}

DEFINE_ART_MODULE(calo::ShowerCalorimetry)

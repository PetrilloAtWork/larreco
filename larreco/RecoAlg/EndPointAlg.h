////////////////////////////////////////////////////////////////////////
/// \file  EndPointAlg.h
/// \brief algorithm to find 2D endpoints
///
/// \author  joshua.spitz@yale.edu
////////////////////////////////////////////////////////////////////////

#ifndef ENDPOINTALG_H
#define ENDPOINTALG_H

#include "art/Framework/Principal/fwd.h"
#include "canvas/Persistency/Common/PtrVector.h"
namespace fhicl { class ParameterSet; }

#include <vector>
#include <string>

namespace recob {
  class Cluster;
  class EndPoint2D;
  class Hit;
}

namespace cluster {

  ///Algorithm to find 2D end points
 class EndPointAlg {

  public:

    explicit EndPointAlg(fhicl::ParameterSet const& pset);

    void   reconfigure(fhicl::ParameterSet const& pset);

    size_t EndPoint(const art::PtrVector<recob::Cluster>           & clusIn,
		    std::vector<recob::EndPoint2D>                 & vtxcol,
		    std::vector< art::PtrVector<recob::Hit> >      & vtxHitsOut,
		    art::Event                                const& evt,
                    std::string                               const& label) const;

  private:

    double Gaussian(int x, int y, double sigma) const;
    double GaussianDerivativeX(int x, int y) const;
    double GaussianDerivativeY(int x, int y) const;
    void VSSaveBMPFile(const char *fileName, unsigned char *pix, int dx, int dy) const;


    int          fTimeBins;
    int          fMaxCorners;
    double       fGsigma;
    int          fWindow;
    double       fThreshold;
    int          fSaveVertexMap;
  };

}



#endif // ENDPOINTALG_H

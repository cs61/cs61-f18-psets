#include "m61.hh"
#include <stdio.h>
#include <assert.h>
#include <string.h>
// Exercise large numbers of allocation sites.

const char* files[200] = {
    "unprovoked.cc", "perissodactylate.cc", "makhzan.cc", "Janthinidae.cc",
    "allothigenetic.cc", "taller.cc", "salmis.cc", "fortuity.cc",
    "monarchically.cc", "cringe.cc", "wolfen.cc", "seem.cc", "pneumological.cc",
    "Mysis.cc", "tumulus.cc", "rhombohedral.cc", "Gold.cc", "nosographical.cc",
    "choice.cc", "quinible.cc", "reintuitive.cc", "unprimed.cc", "hexapla.cc",
    "Salish.cc", "lush.cc", "ignifuge.cc", "bemercy.cc", "Bolelia.cc",
    "thoracobronchotomy.cc", "viewster.cc", "assumption.cc", "travail.cc",
    "progospel.cc", "undropsical.cc", "forjesket.cc", "astrolithology.cc",
    "rooklet.cc", "animalcule.cc", "pulpotomy.cc", "debullition.cc",
    "intransferable.cc", "subterrestrial.cc", "dosimeter.cc",
    "straightforwardness.cc", "unlaundered.cc", "ornamentalize.cc",
    "creatorhood.cc", "urbify.cc", "willies.cc", "ergotic.cc", "demoralization.cc",
    "unpermeable.cc", "sensuousness.cc", "acetation.cc", "scart.cc", "rubican.cc",
    "wronged.cc", "deficience.cc", "libretti.cc", "Brownistic.cc", "zaratite.cc",
    "applesauce.cc", "vetanda.cc", "Luganda.cc", "zoophysiology.cc",
    "epiplectic.cc", "obstructer.cc", "birddom.cc", "cerebral.cc",
    "Melonechinus.cc", "blennothorax.cc", "ramule.cc", "phenolate.cc",
    "phytography.cc", "Esdras.cc", "incruental.cc", "archichlamydeous.cc",
    "Marginella.cc", "foreconceive.cc", "passionful.cc", "analcimite.cc",
    "dharma.cc", "scoleciasis.cc", "scentless.cc", "cliff.cc", "sprawly.cc",
    "oleraceous.cc", "pentanedione.cc", "cendre.cc", "autoreinfusion.cc",
    "ternize.cc", "supraclavicle.cc", "circumstance.cc", "diphtheritic.cc",
    "Didache.cc", "preseminal.cc", "scoldable.cc", "cicisbeism.cc", "schapping.cc",
    "anterior.cc", "ryme.cc", "turion.cc", "boattail.cc", "Artamidae.cc",
    "healder.cc", "portside.cc", "domesticative.cc", "tempered.cc",
    "vaticinate.cc", "Electra.cc", "nectarize.cc", "inturn.cc", "murgeon.cc",
    "exhaustlessly.cc", "holoclastic.cc", "linkboy.cc", "moiety.cc",
    "bletheration.cc", "chartometer.cc", "laughful.cc", "eccentrate.cc",
    "erythrosinophile.cc", "immetricalness.cc", "tangently.cc", "Jeffersonia.cc",
    "subphratry.cc", "dermatotome.cc", "footscald.cc", "myrialitre.cc",
    "amiced.cc", "jaspilite.cc", "butyrolactone.cc", "Shiraz.cc", "misalter.cc",
    "Teutonicism.cc", "ungroundable.cc", "phosphuria.cc", "salmonet.cc",
    "resupinate.cc", "Edoni.cc", "jewel.cc", "mysteriosophy.cc", "tolunitrile.cc",
    "Cantabri.cc", "schizognathous.cc", "apotypic.cc", "Chanca.cc",
    "conversibility.cc", "ergastoplasmic.cc", "biweekly.cc", "Zuni.cc",
    "lignitize.cc", "sharklike.cc", "Stercorariidae.cc", "angiology.cc",
    "Ostyak.cc", "Pakhpuluk.cc", "nocake.cc", "Pullman.cc", "orb.cc", "yardland.cc",
    "Bahmani.cc", "stagnation.cc", "hidling.cc", "seasickness.cc", "underpan.cc",
    "colophonium.cc", "underplan.cc", "extraparental.cc", "steatomatous.cc",
    "rosette.cc", "circumspectively.cc", "Kevyn.cc", "adeem.cc", "eighthly.cc",
    "laundryman.cc", "cellipetal.cc", "floorless.cc", "outthruster.cc",
    "pawpaw.cc", "nubile.cc", "radiocaster.cc", "microcolorimetric.cc",
    "unthrivingly.cc", "ribless.cc", "Phylloxeridae.cc", "Housatonic.cc",
    "rebegin.cc", "queenship.cc", "pobby.cc", "deconcentrate.cc",
    "Gasteromycetes.cc", "prebestow.cc", "printableness.cc", "photozincotypy.cc",
    "unamenable.cc", "fawnery.cc", "radiodetector.cc", "hemoclasis.cc",
    "epepophysis.cc"
};

int main() {
    // array of pointers
    const int nptrs = 200;
    void* ptrs[nptrs];
    for (int i = 0; i != nptrs; ++i) {
        ptrs[i] = nullptr;
    }

    // do 1M allocations from different apparent allocation sites
    for (unsigned i = 0; i != 500000; ++i) {
        const char* file = files[random() % 200];
        int line = 1 + random() % 200;
        void* ptr = m61_malloc(1 + random() % 128, file, line);

        int slot = random() % nptrs;
        m61_free(ptrs[slot], file, line + 3);
        ptrs[slot] = ptr;
    }

    for (int i = 0; i != nptrs; ++i) {
        free(ptrs[i]);
    }

    m61_printstatistics();
}

//! alloc count: active          0   total     500000   fail          0
//! alloc size:  active          0   total        ???   fail        ???

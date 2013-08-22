using System;
using System.IO;
using System.Collections.Generic;
using EQExtractor2.InternalTypes;
using EQExtractor2.OpCodes;
using EQPacket;
using MyUtils;

namespace EQExtractor2.Patches
{
    class PatchAug042011Decoder : PatchMay242011Decoder
    {
        public PatchAug042011Decoder()
        {
            Version = "EQ Client Build Date August 04 2011.";
                        
            ExpectedPPLength = 28496;

            PPZoneIDOffset = 21164;

            PatchConfFileName = "patch_Aug04-2011.conf";
        }                
    }
}
//
//  main.m
//  testMp3Mix
//
//  Created by matthieu campion on 11/23/11.
//

#import <Foundation/Foundation.h>
#include "Mp3Parser.h"

#include <lame/lame.h>
#include </usr/local/include/mpg123.h>

#include <vector>
#include <math.h>

#include "Utils.h"

extern "C"
{
#include "codec.h"
    
#include "system.h"
#include "l3side.h"
#include "loop.h"
#include "huffman.h"
#include "l3bitstream.h"
#include "reservoir.h"
}

#include "nuiAudioConvert.h"

void OnLameError(const char *format, va_list ap);
void OnLameDebug(const char *format, va_list ap);
void OnLameMsg(const char *format, va_list ap);

typedef enum
{
    eLameEncoder = 0,
    eBladeEncoder
} EncoderType;

void writeToFile(NSString* destFileName, NSData* data);

NSData* decode(NSString* srcPath, int nbInFrames, int nbSkipInFrames, bool skipDecoderDelay=true, int channels=2);
bool decodeAndWriteToFile(NSString* srcPath, int nbInFrames, int nbSkipInFrames, NSString* destFileName, bool skipDecoderDelay=true, int channels=2);

NSData* decode(NSData* inData, int nbInFrames, int nbSkipInFrames, bool skipDecoderDelay=true, int channels=2);
bool decodeAndWriteToFile(NSData* inData, int nbInFrames, int nbSkipInFrames, NSString* destFileName, bool skipDecoderDelay=true, int channels=2);

void generateTestSignal(int nbSampleFrames, int nbChannels, std::vector<float>& rBuffer);

NSData* encode(std::vector<float>& buffer, int nbInChannels, EncoderType encoderType = eLameEncoder);
void encodeAndWriteToFile(std::vector<float>& buffer, int nbInChannels, NSString* destFileName, EncoderType encoderType = eLameEncoder);

NSData* encode(std::vector<float>& buffer, int nbInChannels, int nbFrames, int nbSkipFrames, EncoderType encoderType = eLameEncoder);
void encodeAndWriteToFile(std::vector<float>& buffer, int nbInChannels, NSString* destFileName, int nbFrames, int nbSkipFrames, EncoderType encoderType = eLameEncoder);

NSArray* encode_flush(std::vector<float>& buffer, int nbInChannels, int nbFramesBeforeFlush, int nbFramesAfterFlush);


NSData* copyFrames(NSData* inData, int nbFrames, int nbSkipFrames);
NSData* copyFrames(NSString* srcPath, int nbFrames, int nbSkipFrames);
void copyFramesAndWriteToFile(NSString* srcPath, int nbFrames, int nbSkipFrames, NSString* destFileName);

Mp3Parser getParser(NSData* data);

int gNbSampleFramesPerMPEGFrame = 1152;

void test1(NSString* srcFilePath, NSString* destFileName);
void test1_2();
void test1_3();
void test2();
void test3();
void test4();
void test_mono();
void test_blade();
void test_blade_mix(NSString* srcPath, NSString* dstFile);
void test_fadeout(NSString* srcPath1, NSString* dstFile);
void test_crossfade(NSString* srcPath1, NSString* srcPath2, NSString* dstFile);
void test_nogap(NSString* mp3Path);
void test_minimal_preflush(NSString* mp3Path, int A_nbFrames, int B_nbFrames, int preflush_nbFrames);
void test_double_jonsction(NSString* mp3Path, int A_nbFrames, int B_nbFrames, int C_nbFrames, int preflush_nbFrames);

int main (int argc, const char * argv[])
{
  @autoreleasepool 
  {            
//      test_nogap(@"/Users/mat/work/dev/yastream/tests/testMp3Mix/testMp3Mix/resources/mercy.mp3");
      
//      test_minimal_preflush(@"/Users/mat/work/dev/yastream/tests/testMp3Mix/testMp3Mix/resources/mercy.mp3", 995, 1000, 2);
      
      test_double_jonsction(@"/Users/mat/work/dev/yastream/tests/testMp3Mix/testMp3Mix/resources/please.mp3", 995, 1000, 1000, 2);
  }
  return 0;
}

void test_double_jonsction(NSString* mp3Path, int A_nbFrames, int B_nbFrames, int C_nbFrames, int preflush_nbFrames)
{
    int channels = 2;
    int samplesPerMPEGFrame = gNbSampleFramesPerMPEGFrame * channels;
    int bytesPerMPEGFrame = samplesPerMPEGFrame * sizeof(float);
    
    NSData* A_copy = copyFrames(mp3Path, A_nbFrames, 0);
    
    {
        writeToFile(@"A_copy.mp3", A_copy);
    }
    
    int preflush_plus_B_nbFrames = preflush_nbFrames + B_nbFrames;
    int nbFeedFrames = preflush_plus_B_nbFrames + 10;
    int nbSkipFrames = A_nbFrames - preflush_nbFrames;
    NSData* preflush_plus_B_decoded = decode(mp3Path, nbFeedFrames, nbSkipFrames);
    
    unsigned char* pSrc = (unsigned char*)preflush_plus_B_decoded.bytes + 3 * (bytesPerMPEGFrame / 2);
    
    std::vector<float> preflush_plus_B_wav;
    preflush_plus_B_wav.resize(preflush_plus_B_nbFrames * samplesPerMPEGFrame);
    memcpy(&preflush_plus_B_wav[0], pSrc, preflush_plus_B_nbFrames * bytesPerMPEGFrame);
    
    NSArray* encoded_datas = encode_flush(preflush_plus_B_wav, channels, preflush_nbFrames, B_nbFrames);
    NSData* B_flush_encoded = [encoded_datas objectAtIndex:1];
    
    {
        writeToFile(@"B_flush_encoded.mp3", B_flush_encoded);
    }
    
    NSMutableData* A_copy_plus_B_flush = [NSMutableData data];
    [A_copy_plus_B_flush appendData:A_copy];
    [A_copy_plus_B_flush appendData:B_flush_encoded];
    
    {
        writeToFile(@"A_copy+B_flush.mp3", A_copy_plus_B_flush);
    }
    
    NSData* C_copy = copyFrames(mp3Path, C_nbFrames, A_nbFrames + B_nbFrames);
    
    {
        writeToFile(@"C_copy.mp3", C_copy);
    }
    
    NSMutableData* total = [NSMutableData data];
    [total appendData:A_copy];
    [total appendData:B_flush_encoded];
    [total appendData:C_copy];

    {
        writeToFile(@"total.mp3", total);
    }
}

void test_minimal_preflush(NSString* mp3Path, int A_nbFrames, int B_nbFrames, int preflush_nbFrames)
{
    int channels = 2;
    int samplesPerMPEGFrame = gNbSampleFramesPerMPEGFrame * channels;
    int bytesPerMPEGFrame = samplesPerMPEGFrame * sizeof(float);
    
    NSData* A_copy = copyFrames(mp3Path, A_nbFrames, 0);
    
    {
        writeToFile(@"A_copy.mp3", A_copy);
    }
    
    int preflush_plus_B_nbFrames = preflush_nbFrames + B_nbFrames;
    int nbFeedFrames = preflush_plus_B_nbFrames + 10;
    int nbSkipFrames = A_nbFrames - preflush_nbFrames;
    NSData* preflush_plus_B_decoded = decode(mp3Path, nbFeedFrames, nbSkipFrames);
    
    unsigned char* pSrc = (unsigned char*)preflush_plus_B_decoded.bytes + 3 * (bytesPerMPEGFrame / 2);
    
    std::vector<float> preflush_plus_B_wav;
    preflush_plus_B_wav.resize(preflush_plus_B_nbFrames * samplesPerMPEGFrame);
    memcpy(&preflush_plus_B_wav[0], pSrc, preflush_plus_B_nbFrames * bytesPerMPEGFrame);
    
    NSArray* encoded_datas = encode_flush(preflush_plus_B_wav, channels, preflush_nbFrames, B_nbFrames);
    NSData* B_flush_encoded = [encoded_datas objectAtIndex:1];
    
    {
        writeToFile(@"B_flush_encoded.mp3", B_flush_encoded);
    }
    
    NSMutableData* A_copy_plus_B_flush = [NSMutableData data];
    [A_copy_plus_B_flush appendData:A_copy];
    [A_copy_plus_B_flush appendData:B_flush_encoded];
    
    {
        writeToFile(@"A_copy+B_flush.mp3", A_copy_plus_B_flush);
    }
}

void test_nogap(NSString* mp3Path)
{
    int channels = 2;
    int samplesPerMPEGFrame = gNbSampleFramesPerMPEGFrame * channels;
    int bytesPerMPEGFrame = samplesPerMPEGFrame * sizeof(float);
    
    int A_frames = 993;
    int B_frames = 1000;
    int AB_frames = A_frames + B_frames;
    
    NSData* A_copy = copyFrames(mp3Path, A_frames, 0);
    NSData* B_copy = copyFrames(mp3Path, B_frames, A_frames);
    NSMutableData* AB_copy = [NSMutableData data];
    [AB_copy appendData:A_copy];
    [AB_copy appendData:B_copy];
    
    {
        writeToFile(@"A_copy.mp3", A_copy);
        writeToFile(@"B_copy.mp3", B_copy);
        writeToFile(@"A_copy+B_copy.mp3", AB_copy);
    }
    
    NSData* AB_decoded = decode(mp3Path, A_frames + B_frames + 10, 0);
    {
        writeToFile(@"AB_decoded.raw", AB_decoded);
    }
    
    std::vector<float> AB_wav;
    AB_wav.resize(AB_frames * samplesPerMPEGFrame);
    memcpy(&AB_wav[0], AB_decoded.bytes, AB_frames * bytesPerMPEGFrame);

    std::vector<float> A_wav;
    A_wav.resize(A_frames * samplesPerMPEGFrame);
    memcpy(&A_wav[0], &AB_wav[0], A_frames * bytesPerMPEGFrame);
    
    std::vector<float> B_wav;
    B_wav.resize(B_frames * samplesPerMPEGFrame);
    memcpy(&B_wav[0], &AB_wav[A_frames * samplesPerMPEGFrame], B_frames * bytesPerMPEGFrame);
    
    {
        writeToFile(@"AB_wav.raw", [NSData dataWithBytes:&AB_wav[0] length:AB_wav.size() * sizeof(float)]);
        writeToFile(@"A_wav.raw", [NSData dataWithBytes:&A_wav[0] length:A_wav.size() * sizeof(float)]);
        writeToFile(@"B_wav.raw", [NSData dataWithBytes:&B_wav[0] length:B_wav.size() * sizeof(float)]);
    }
    
    NSData* AB_reencoded = encode(AB_wav, channels);
    NSData* A_reencoded = encode(A_wav, channels);
    NSData* B_reencoded = encode(B_wav, channels);
    NSMutableData* A_reencoded_plus_B_reencoded = [NSMutableData data];
    [A_reencoded_plus_B_reencoded appendData:A_reencoded];
    [A_reencoded_plus_B_reencoded appendData:B_reencoded];
    
    {
        writeToFile(@"AB_reencoded.mp3", AB_reencoded);
        writeToFile(@"A_reencoded.mp3", A_reencoded);
        writeToFile(@"B_reencoded.mp3", B_reencoded);
        writeToFile(@"A_reencoded+B_reencoded.mp3", A_reencoded_plus_B_reencoded);
    }
    
    NSArray* flush_encoded_array = encode_flush(AB_wav, channels, A_frames, B_frames);
    NSData* A_flush_encoded = [flush_encoded_array objectAtIndex:0];
    NSData* B_flush_encoded = [flush_encoded_array objectAtIndex:1];
    NSMutableData* A_flush_plus_B_flush = [NSMutableData data];
    [A_flush_plus_B_flush appendData:A_flush_encoded];
    [A_flush_plus_B_flush appendData:B_flush_encoded];
    
    {
        writeToFile(@"A_flush.mp3", A_flush_encoded);
        writeToFile(@"B_flush.mp3", B_flush_encoded);
        writeToFile(@"A_flush+B_flush.mp3", A_flush_plus_B_flush);
    }
    
    NSMutableData* A_copy_plus_B_flush = [NSMutableData data];
    [A_copy_plus_B_flush appendData:A_copy];
    [A_copy_plus_B_flush appendData:B_flush_encoded];
    {
        writeToFile(@"A_copy+B_flush.mp3", A_copy_plus_B_flush);
    }
    
    NSData* A_copy_plus_B_fluash_minus_Xing = copyFrames(A_copy_plus_B_flush, AB_frames, 0);
    {
        writeToFile(@"A_copy+B_flush-Xing.mp3", A_copy_plus_B_fluash_minus_Xing);
    }
    
//    int nbBytes = A_copy_plus_B_flush.length;
//    const char* bytes = (const char*)A_copy_plus_B_flush.bytes;
//    
//    nglIMemory stream(bytes, nbBytes);
//    Mp3Parser parser(stream, false, true);
//    bool ok = true;
//    while (ok)
//    {
//        const Mp3Frame& f = parser.GetCurrentFrame();
//        int byteLength = f.GetByteLength();
//        NSLog(@"frame length = %d bytes", byteLength);
//        ok = parser.GoToNextFrame();
//    }
    
}

void test_crossfade(NSString* srcPath1, NSString* srcPath2, NSString* dstFile)
{
    int channels = 2;
    
    int nbFramesJoint = 1;
    int nbFramesToCopy = 100;
    NSData* copy = copyFrames(srcPath1, nbFramesToCopy, 0);
    
    int nbFramesToEncode = 300;
    int nbFeedDecodeFrames = nbFramesToEncode + 10;
    
    NSData* wavData1 = decode(srcPath1, nbFeedDecodeFrames, nbFramesToCopy - nbFramesJoint);
    NSData* wavData2 = decode(srcPath2, nbFeedDecodeFrames, 0);
    
    std::vector<float> wav1;
    std::vector<float> wav2;
    wav1.resize((0.5 + nbFramesToEncode) * gNbSampleFramesPerMPEGFrame * channels);
    wav2.resize((0.5 + nbFramesToEncode) * gNbSampleFramesPerMPEGFrame * channels);
    
    float* pSrc1 = (float*)wavData1.bytes + (int)(0.5 * gNbSampleFramesPerMPEGFrame * channels);
    float* pSrc2 = (float*)wavData2.bytes;
    
    memcpy(&wav1[0], pSrc1, wav1.size() * sizeof(float));
    memcpy(&wav2[0], pSrc2, wav2.size() * sizeof(float));
    
    int crossfadeLength = 3 * 44100;
    
    std::vector<float> wav;
    wav.resize(wav1.size());
    
    int nbSampleFrames = wav1.size() / channels;
    crossfadeLength = MIN(crossfadeLength, nbSampleFrames);
    for (int i = 0; i < nbSampleFrames; i++)
    {
        if (i < crossfadeLength)
        {
            float mult1 = 1 - (float)i / (float)crossfadeLength;
            float mult2 = (float)i / (float)crossfadeLength;
            for (int c = 0; c < channels; c++)
            {
                wav[i * channels + c] = mult1 * wav1[i * channels + c] + mult2 * wav2[i * channels + c];
            }
        }
        else 
        {
            for (int c = 0; c < channels; c++)
            {
                wav[i * channels + c] = wav2[i * channels + c];
            }
        }
        
    } 
    
    writeToFile(@"wav.raw", [NSData dataWithBytes:&wav[0] length:wav.size() * sizeof(float)]);
    
    NSData* encoded = encode(wav, channels, nbFramesToEncode, nbFramesJoint, eLameEncoder);
    
    NSMutableData* outData = [NSMutableData data];
    [outData appendData:copy];
    [outData appendData:encoded];
    
    writeToFile(dstFile, outData);
}

void test_fadeout(NSString* srcPath, NSString* dstFile)
{
    int channels = 2;
    
    int nbFramesToEncode = 500;
    int nbFeedDecodeFrames = nbFramesToEncode + 10;
    NSData* decoded = decode(srcPath, nbFeedDecodeFrames, 0);
    
    std::vector<float> wav;
    wav.resize(nbFramesToEncode * gNbSampleFramesPerMPEGFrame * channels);
    memcpy(&wav[0], decoded.bytes, wav.size() * sizeof(float));

    
    int nbSampleFrames1 = wav.size() / channels;
    for (int i = 0; i < nbSampleFrames1; i++)
    {
        float mult = 1 - (float)i / (float)nbSampleFrames1;
        for (int c = 0; c < channels; c++)
        {
            wav[i * channels + c] *= mult;
        }
    }
    
    writeToFile(@"wav.raw", [NSData dataWithBytes:&wav[0] length:wav.size() * sizeof(float)]);
    
    NSData* encoded = encode(wav, channels, nbFramesToEncode, 0, eBladeEncoder);
    
    writeToFile(dstFile, encoded);
}

void test_blade_mix(NSString* srcPath, NSString* dstFile)
{
    int channels = 2;
    
    int nbFramesJoint = 1 + 2;
    int nbFramesToCopy = 100;
    NSData* copy = copyFrames(srcPath, nbFramesToCopy, 0);
    
    int nbFramesToEncode = 100;
    int nbFeedDecodeFrames = nbFramesToEncode + 10;
    NSData* decoded = decode(srcPath, nbFeedDecodeFrames, nbFramesToCopy - nbFramesJoint);
    
    
    std::vector<float> wav;
    wav.resize((0.5 + nbFramesToEncode) * gNbSampleFramesPerMPEGFrame * channels);
    
    float* pSrc = ((float*)decoded.bytes) + (int)(0.5 * gNbSampleFramesPerMPEGFrame * channels);
    
    memcpy(&wav[0], pSrc, wav.size() * sizeof(float));
    
    NSData* encoded = encode(wav, channels, nbFramesToEncode, nbFramesJoint, eBladeEncoder);
    
    NSMutableData* outData = [NSMutableData data];
    [outData appendData:copy];
    [outData appendData:encoded];
    
    writeToFile(dstFile, outData);
}

void test_blade()
{
    int nbSampleFrames = gNbSampleFramesPerMPEGFrame * 10;
    int nbChannels = 2;
    std::vector<float> generated;
    generateTestSignal(nbSampleFrames, nbChannels, generated);
    
    std::vector<float> signal;
    signal.resize(50 * nbChannels + generated.size());
    for (int i = 0; i < 50; i++)
    {
        for (int c = 0; c < nbChannels; c++)
        {
            signal[i * nbChannels + c] = 0;
        }
    }
    memcpy(&signal[50 * nbChannels], &generated[0], generated.size() * sizeof(float));
    
    encodeAndWriteToFile(generated, nbChannels, @"lame-encoded.mp3", eLameEncoder);
    encodeAndWriteToFile(signal, nbChannels, @"blade-encoded.mp3", eBladeEncoder);
}

void test4()
{
    NSString* src = @"/Users/mat/work/dev/yastream/tests/testMp3Mix/testMp3Mix/resources/test.mp3";
    
    NSData* f1 = copyFrames(src, 1, 0);
    NSMutableData* d = [NSMutableData data];
    
    for (int i = 0 ; i < 10; i++)
        [d appendData:f1];
    
    writeToFile(@"frame1_x3.mp3", d);
}

void test3()
{
    NSString* src = @"/Users/mat/work/dev/yastream/tests/testMp3Mix/testMp3Mix/resources/test.mp3";
    
    copyFramesAndWriteToFile(src, 8, 0, @"copy-8-0.mp3");
    
    NSData* d1 = copyFrames(src, 8, 0);
    decodeAndWriteToFile(d1, 8, 0, @"copy-decode-8-0.raw", false);
    
    
    copyFramesAndWriteToFile(src, 8, 1, @"copy-8-1.mp3");
    copyFramesAndWriteToFile(src, 8, 2, @"copy-8-2.mp3");
    copyFramesAndWriteToFile(src, 8, 3, @"copy-8-3.mp3");
}

void test2()
{
    int channels = 2;
    int samplesPerFrame = gNbSampleFramesPerMPEGFrame * channels;
    
    int nbWhiteFrames = 10;
    int nbSignalFrames = 10;
    
    std::vector<float> white;
    white.resize(nbWhiteFrames * samplesPerFrame);
    for (int i = 0; i < white.size(); i++)
        white[i] = 0;
    
    for (int i = 0; i < 2000; i++)
    {
        white[i] = 0.5;
        white[white.size() - 1- i] = 0.5;
    }
    
    std::vector<float> signal;
    generateTestSignal(nbSignalFrames * gNbSampleFramesPerMPEGFrame, channels, signal);
    
    std::vector<float> input;
    input.resize(white.size() + signal.size());
    memcpy(&input[0], &white[0], white.size() * sizeof(float));
    memcpy(&input[white.size()], &signal[0], signal.size() * sizeof(float));
    
    NSData* inputData = [NSData dataWithBytes:&input[0] length:input.size() * sizeof(float)];
    writeToFile(@"input.raw", inputData);
    
    NSData* whiteEncoded = encode(white, channels);
    NSData* signalEncoded = encode(signal, channels);
    NSData* totalEncoded = encode(input, channels);
    
    writeToFile(@"white.mp3", whiteEncoded);
    writeToFile(@"signal.mp3", signalEncoded);
    writeToFile(@"total.mp3", totalEncoded);
}

void test_mono()
{
    NSString* src = @"/Users/mat/work/dev/yastream/tests/testMp3Mix/testMp3Mix/resources/please-mono.mp3";
    
    int channels = 1;
    int samplesPerFrame = gNbSampleFramesPerMPEGFrame * channels;
    
    int nbCopyFrames = 101;
    int nbJointFrames = 1;
    int nbEncodedFrames = 100;
    
    NSData* copied = copyFrames(src, nbCopyFrames, 0);
    
    writeToFile(@"copied-mono.mp3", copied);
    
    int nbFeedDecodeFrames = 200;
    
    //    NSData* wavData = decode(src, nbFeedDecodeFrames, nbCopyFrames - nbJointFrames);
    //    
    //    std::vector<float> wav;
    ////    wav.resize(nbFeedDecodeFrames * samplesPerFrame);
    ////    memcpy(&wav[0], wavData.bytes, nbFeedDecodeFrames * samplesPerFrame * sizeof(float));
    //
    //    wav.resize((0.5 + nbFeedDecodeFrames) * samplesPerFrame);
    //    
    //    memset(&wav[0], 0, (samplesPerFrame / 2) * sizeof(float));
    //    memcpy(&wav[samplesPerFrame / 2], wavData.bytes, nbFeedDecodeFrames * samplesPerFrame * sizeof(float));
    
    NSData* wavData = decode(src, nbFeedDecodeFrames, nbCopyFrames - nbJointFrames - 1, true, channels);
    
    int samplesToCopy = (0.5 + nbFeedDecodeFrames) * samplesPerFrame;
    std::vector<float> wav;
    wav.resize(samplesToCopy);
    float* pSrc = ((float*)wavData.bytes) + (samplesPerFrame / 2);
    
    memcpy(&wav[0], pSrc, samplesToCopy * sizeof(float));
    
    NSData* encoded = encode(wav, channels, nbEncodedFrames, nbJointFrames+1);
    
    writeToFile(@"encoded-mono.mp3", encoded);
    
    NSMutableData* outData = [NSMutableData data];
    [outData appendData:copied];
    [outData appendData:encoded];
    
    writeToFile(@"glue-mono.mp3", outData);
}

void test1(NSString* srcFilePath, NSString* destFileName)
{    
    int channels = 2;
    int samplesPerFrame = gNbSampleFramesPerMPEGFrame * channels;
    
    int nbCopyFrames = 100;
    int nbJointFrames = 1 + 1;
    int nbEncodedFrames = 100;
    
    NSData* copied = copyFrames(srcFilePath, nbCopyFrames, 0);
    
    writeToFile(@"copied.mp3", copied);
    
    int nbFeedDecodeFrames = 200;

    NSData* wavData = decode(srcFilePath, nbFeedDecodeFrames, nbCopyFrames - nbJointFrames - 1);
    
    int samplesToCopy = (0.5 + nbFeedDecodeFrames) * samplesPerFrame;
    std::vector<float> wav;
    wav.resize(samplesToCopy);
    float* pSrc = ((float*)wavData.bytes) + (samplesPerFrame / 2);
    
    memcpy(&wav[0], pSrc, samplesToCopy * sizeof(float));
    
    int fadeLength = gNbSampleFramesPerMPEGFrame;
    for (int i = 0; i < fadeLength; i++)
    {
        float mult = (float)i / (float)fadeLength;
        for (int c = 0; c < channels; c++)
        {
            wav[i * channels + c] *= mult;
        }
    }
    
    NSData* encoded = encode(wav, channels, nbEncodedFrames, nbJointFrames+1);
    
    writeToFile(@"encoded.mp3", encoded);
    
    NSMutableData* outData = [NSMutableData data];
    [outData appendData:copied];
    [outData appendData:encoded];
    
    writeToFile(destFileName, outData);
}

void test1_2()
{
    NSString* src = @"/Users/mat/work/dev/yastream/tests/testMp3Mix/testMp3Mix/resources/test.mp3";
    
    int channels = 2;
    int samplesPerFrame = gNbSampleFramesPerMPEGFrame * channels;
    
    int nbCopyFrames = 4;
    int nbJointFrames = 1;
    
    NSData* copied = copyFrames(src, nbCopyFrames, 0);
    
    writeToFile(@"copied.mp3", copied);
    
    int nbFeedDecodeFrames = 4;
    NSData* wavData = decode(src, nbFeedDecodeFrames, nbCopyFrames - nbJointFrames, true);
    
    writeToFile(@"decoded.raw", wavData);	
    
    float* granule = ((float*)wavData.bytes);
    
    int nbGeneratedFrames = 8;
    std::vector<float> generated;
    generateTestSignal(nbGeneratedFrames * gNbSampleFramesPerMPEGFrame, channels, generated);
    
    std::vector<float> wav;
    wav.resize((samplesPerFrame / 2) + generated.size());

    memcpy(&wav[0], granule, (samplesPerFrame / 2) * sizeof(float));
    memcpy(&wav[samplesPerFrame / 2], &generated[0], generated.size() * sizeof(float));
    
    NSData* w = [NSData dataWithBytes:&wav[0] length:wav.size() * sizeof(float)];
    writeToFile(@"encoder_in.raw", w);

    NSData* encoded = encode(wav, channels, nbGeneratedFrames, nbJointFrames);
    
    writeToFile(@"encoded.mp3", encoded);
    
    NSMutableData* outData = [NSMutableData data];
    [outData appendData:copied];
    [outData appendData:encoded];
    
    writeToFile(@"glue.mp3", outData);
}

void test1_3()
{
    NSString* src = @"/Users/mat/work/dev/yastream/tests/testMp3Mix/testMp3Mix/resources/test.mp3";
    
    int channels = 2;
    int nbSamplesPerFrame = gNbSampleFramesPerMPEGFrame * channels;
    int nbCopyFrames = 4;
    int nbEncodedFrames = 8;
    
    NSData* copied = copyFrames(src, nbCopyFrames, 0);
    
    writeToFile(@"copied.mp3", copied);
    
    int nbGeneratedFrames = 8;
    std::vector<float> generated;
    generateTestSignal(nbGeneratedFrames * gNbSampleFramesPerMPEGFrame, channels, generated);
//    generated.resize(nbGeneratedFrames * nbSamplesPerFrame);
    for (int i = 0; i < generated.size(); i++)
    {
        generated[i] *= 0.1;
    }
    
        
    
    NSData* generatedData = [NSData dataWithBytes:&generated[0] length:generated.size() * sizeof(float)];
    writeToFile(@"generated.raw", generatedData);
    
    NSData* encoded = encode(generated, channels, nbEncodedFrames, 1);
    writeToFile(@"encoded.mp3", encoded);
    
    NSMutableData* outData = [NSMutableData data];
    [outData appendData:copied];
    [outData appendData:encoded];
    
    writeToFile(@"glue.mp3", outData);
}

void generateTestSignal(int nbSampleFrames, int nbChannels, std::vector<float>& rBuffer)
{
    int period = 192;
    float square[period];
    float triangle[period];
    float sine[period];
    float square2[period];
    float triangle2[period];
    float sine2[period];
    float coeff = 0.7;
    int nbStepsInCycle = 6;
    int nbSampleFramesInCycle = nbStepsInCycle * period;
    
    for (int i = 0; i < period; i++)
    {
        float val = (i < period / 2) ? 1 : -1;
        square[i] = val;
        square2[i] = coeff * val;
        
        if (i <= (float)period / 4.f)
        {
            val = (float)i / ((float)period / 4.f);
        }
        else if (i <= 3.f * ((float)period / 4.f))
        {
            val = 1.f - 2.f * (((float)i - (float)period/4.f) / ((float)period/2.f));
        }
        else 
        {
            val = -1.f + ((float)i - 3.f * ((float)period/4.f)) / ((float)period/4.f);
        }
        triangle[i] = val;
        triangle2[i] = coeff * val;
        
        val = sinf(2 * M_PI * i / period);
        sine[i] = val;
        sine2[i] = coeff * val;
    }
    
    rBuffer.resize(nbSampleFrames * nbChannels);
    
    for (int f = 0; f < nbSampleFrames; f++)
    {
        int cycleOffset = f % nbSampleFramesInCycle;
        int stepIndex = floorf((float)cycleOffset / (float)period);
        int stepOffset = cycleOffset % period;
        
        float val;
        if (stepIndex == 0)
            val = square[stepOffset];
        else if (stepIndex == 1)
            val = triangle[stepOffset];
        else if (stepIndex == 2)
            val = sine[stepOffset];
        else if (stepIndex == 3)
            val = square2[stepOffset];
        else if (stepIndex == 4)
            val = triangle2[stepOffset];
        else if (stepIndex == 5)
            val = sine2[stepOffset];
        
        for (int c = 0; c < nbChannels; c++)
            rBuffer[f * nbChannels + c] = val;
    }
}

NSArray* encode_flush(std::vector<float>& buffer, int nbInChannels, int nbFramesBeforeFlush, int nbFramesAfterFlush)
{
    float samplerate = 44100;
    float bitrate = 192;
    
    MPEG_mode mode;
    if (nbInChannels == 1)
        mode = (MPEG_mode)3;
    else if (nbInChannels == 2)
        mode = (MPEG_mode)1;
    else
        return nil;
    
    // LAME
    lame_global_flags* lameFlags;
    lameFlags = lame_init();
    lame_set_in_samplerate(lameFlags, samplerate);
    lame_set_num_channels(lameFlags, nbInChannels);
    lame_set_mode(lameFlags, mode);
    lame_set_scale(lameFlags, 1.0f);
    lame_set_out_samplerate(lameFlags, samplerate);
    lame_set_bWriteVbrTag(lameFlags, 0);
    lame_set_quality(lameFlags, 0);
    lame_set_errorf(lameFlags, OnLameError);
    lame_set_debugf(lameFlags, OnLameDebug);
    lame_set_msgf(lameFlags, OnLameMsg);
    lame_set_brate(lameFlags, bitrate);
    lame_init_params(lameFlags);
    lame_print_config(lameFlags);
    
    vbr_mode vbr = lame_get_VBR(lameFlags);
    
    int nogap = lame_get_nogap_total(lameFlags);
    lame_set_nogap_total(lameFlags, 1);
    nogap = lame_get_nogap_total(lameFlags);
    
    int disable_reservoir = lame_get_disable_reservoir(lameFlags);
    lame_set_disable_reservoir(lameFlags, 1);
    disable_reservoir = lame_get_disable_reservoir(lameFlags);
    
    NSMutableData* encoded1 = [NSMutableData data];
    NSMutableData* encoded2 = [NSMutableData data];
    NSMutableData* encoded_current = encoded1;
    
    int inNbSampleFrames = buffer.size() / nbInChannels;
    
    int mp3buf_size = 1.25 * 1152 + 7200;
    unsigned char mp3buf[mp3buf_size];
    
    int toFeed = inNbSampleFrames;
    int inputOffset = 0;
    int res = 1;
    int framesDone = 0;
    while (toFeed > 0 || res > 0)
    {
        int nbInputSampleFrames = MIN(1152, toFeed);
        const float* input = &buffer[inputOffset * nbInChannels];
        
        if (nbInChannels == 1)
            res = lame_encode_buffer_ieee_float(lameFlags, input, input, nbInputSampleFrames, mp3buf, mp3buf_size);
        else
            res = lame_encode_buffer_interleaved_ieee_float(lameFlags, input, nbInputSampleFrames, mp3buf, mp3buf_size);
        if (res >= 0)
        {
            NSLog(@"%d bytes encoded", res);
            [encoded_current appendBytes:mp3buf length:res];
        }
        else
            NSLog(@"encode error = %d", res);
        
        inputOffset += nbInputSampleFrames;
        toFeed -= nbInputSampleFrames;
        framesDone++;
        
        if (framesDone == nbFramesBeforeFlush)
        {
            res = lame_encode_flush_nogap(lameFlags, mp3buf, mp3buf_size);
            [encoded_current appendBytes:mp3buf length:res];
            encoded_current = encoded2;
        }
    }
    
    res = 1;
    while (res > 0)
    {
        res = lame_encode_flush_nogap(lameFlags, mp3buf, mp3buf_size);
        [encoded_current appendBytes:mp3buf length:res];
        NSLog(@"flush: %d bytes", res);
    }
    
    
    NSArray* array = [NSArray arrayWithObjects:encoded1, encoded2, nil];
    return array;
}

NSData* encode(std::vector<float>& buffer, int nbInChannels, EncoderType encoderType)
{
    float samplerate = 44100;
    float bitrate = 192;
    
    
    if (encoderType == eLameEncoder)
    {
        MPEG_mode mode;
        if (nbInChannels == 1)
            mode = (MPEG_mode)3;
        else if (nbInChannels == 2)
            mode = (MPEG_mode)1;
        else
            return nil;
        
        // LAME
        lame_global_flags* lameFlags;
        lameFlags = lame_init();
        lame_set_in_samplerate(lameFlags, samplerate);
        lame_set_num_channels(lameFlags, nbInChannels);
        lame_set_mode(lameFlags, mode);
        lame_set_scale(lameFlags, 1.0f);
        lame_set_out_samplerate(lameFlags, samplerate);
        lame_set_bWriteVbrTag(lameFlags, 0);
        lame_set_quality(lameFlags, 0);
        lame_set_errorf(lameFlags, OnLameError);
        lame_set_debugf(lameFlags, OnLameDebug);
        lame_set_msgf(lameFlags, OnLameMsg);
        lame_set_brate(lameFlags, bitrate);
        lame_init_params(lameFlags);
        lame_print_config(lameFlags);
        
        int nogap = lame_get_nogap_total(lameFlags);
        lame_set_nogap_total(lameFlags, 1);
        nogap = lame_get_nogap_total(lameFlags);
        
        int disable_reservoir = lame_get_disable_reservoir(lameFlags);
        lame_set_disable_reservoir(lameFlags, 1);
        disable_reservoir = lame_get_disable_reservoir(lameFlags);
        
        NSMutableData* outData = [NSMutableData data];
        
        int inNbSampleFrames = buffer.size() / nbInChannels;
        
        int toFeed = inNbSampleFrames;
        int inputOffset = 0;
        int res = 1;
        while (toFeed > 0 || res > 0) 
        {
            int nbInputSampleFrames = MIN(1152, toFeed);
            const float* input = &buffer[inputOffset * nbInChannels];
            
            int mp3buf_size = 1.25 * 1152 + 7200;
            unsigned char mp3buf[mp3buf_size];
            if (nbInChannels == 1)
                res = lame_encode_buffer_ieee_float(lameFlags, input, input, nbInputSampleFrames, mp3buf, mp3buf_size);
            else
                res = lame_encode_buffer_interleaved_ieee_float(lameFlags, input, nbInputSampleFrames, mp3buf, mp3buf_size);
            if (res >= 0)
            {
                NSLog(@"%d bytes encoded", res);
                [outData appendBytes:mp3buf length:res];
            }
            else
                NSLog(@"encode error = %d", res);
            
            inputOffset += nbInputSampleFrames;
            toFeed -= nbInputSampleFrames;
        }
        
        
        
        return outData;
    }
    else if (encoderType == eBladeEncoder)
    {
        CodecInitIn codecInitIn;
        codecInitIn.frequency = samplerate;
        codecInitIn.bitrate = bitrate;
        codecInitIn.mode = (nbInChannels == 1) ? 3/*mono*/ : 0/*stereo*/;
        codecInitIn.fCopyright = 0;
        codecInitIn.emphasis = 0;
        codecInitIn.fCRC = 0;
        codecInitIn.fOriginal = 0;
        codecInitIn.fPrivate = 0;

        CodecInitOut* pCodecinitOut = codecInit(&codecInitIn);
        
        NSMutableData* outData = [NSMutableData data];
        
        int inNbSampleFrames = buffer.size() / nbInChannels;
        
        int toFeed = inNbSampleFrames;
        int inputOffset = 0;
        unsigned int res = 1;
        int16* pInt16Samples = new int16[1152 * nbInChannels];
        while (toFeed > 0) 
        {
            int nbInputSampleFrames = MIN(1152, toFeed);
            float* input = &buffer[inputOffset * nbInChannels];
            
            int mp3buf_size = 1.25 * 1152 + 7200;
            char mp3buf[mp3buf_size];
            
//            fixStatic_reservoir();
            nuiAudioConvert_FloatBufferTo16bits(input, pInt16Samples, nbInputSampleFrames * nbInChannels);
            res = codecEncodeChunk(nbInputSampleFrames, pInt16Samples, mp3buf);

            NSLog(@"%d bytes encoded", res);
            [outData appendBytes:mp3buf length:res];
            
            inputOffset += nbInputSampleFrames;
            toFeed -= nbInputSampleFrames;
        }
        delete[] pInt16Samples;
        
        return outData;
    }
    
    return nil;
}


NSData* encode(std::vector<float>& buffer, int nbInChannels, int nbFrames, int nbSkipFrames, EncoderType encoderType)
{
    NSData* data = encode(buffer, nbInChannels, encoderType);
    int nbBytes = data.length;
    const char* bytes = (const char*)data.bytes;
    
    nglIMemory stream(bytes, nbBytes);
    Mp3Parser parser(stream, false, true);
    
    NSMutableData* outData = [NSMutableData data];
    
    int f = 0;
    int skip = nbSkipFrames;
    bool ok = true;
    while (f < nbFrames && ok) 
    {
        Mp3Frame frame = parser.GetCurrentFrame();
        if (frame.IsValid() && !frame.GetHeader().mIsXing)
        {
            if (skip == 0)
            {
                int b = frame.GetByteLength();
                std::vector<uint8> buffer;
                buffer.resize(b);
                parser.ReadFrameBytes(buffer);
                [outData appendBytes:&buffer[0] length:b];
                f++;
            }
            else
            {
                skip--;
            }
        }
        else 
        {
            NSLog(@"frame is not valid");
        }
        ok = parser.GoToNextFrame();
    }
    
    return outData;
}

void encodeAndWriteToFile(std::vector<float>& buffer, int nbInChannels, NSString* destFileName, EncoderType encoderType)
{
    NSData* data = encode(buffer, nbInChannels, encoderType);
    writeToFile(destFileName, data);
}

void encodeAndWriteToFile(std::vector<float>& buffer, int nbInChannels, NSString* destFileName, int nbFrames, int nbSkipFrames, EncoderType encoderType)
{
    NSData* data = encode(buffer, nbInChannels, nbFrames, nbSkipFrames, encoderType);
    writeToFile(destFileName, data);
}

NSData* copyFrames(NSData* inData, int nbFrames, int nbSkipFrames)
{
    Mp3Parser parser = getParser(inData);
    
    NSMutableData* outData = [NSMutableData data];
    
    
    int f = 0;
    int skip = nbSkipFrames;
    bool ok = true;
    while (f < nbFrames && ok)
    {
        Mp3Frame frame = parser.GetCurrentFrame();
        if (frame.IsValid() && !frame.GetHeader().mIsXing)
        {
            if (skip == 0)
            {
                f++;
                int bytes = frame.GetByteLength();
                std::vector<uint8> data;
                data.resize(bytes);
                parser.ReadFrameBytes(data);
                [outData appendBytes:&data[0] length:bytes];
            }
            else
            {
                skip--;
            }
        }
        ok = parser.GoToNextFrame();
    }
    
    return outData;

}
NSData* copyFrames(NSString* srcPath, int nbFrames, int nbSkipFrames)
{
    NSData* inData = [NSData dataWithContentsOfFile:srcPath];
    NSData* outData = copyFrames(inData, nbFrames, nbSkipFrames);
    return outData;
}

void copyFramesAndWriteToFile(NSString* srcPath, int nbFrames, int nbSkipFrames, NSString* destFileName)
{
    NSData* data = copyFrames(srcPath, nbFrames, nbSkipFrames);
    writeToFile(destFileName, data);
}


NSData* decode(NSData* inData, int nbInFrames, int nbSkipInFrames, bool skipDecoderDelay, int channels)
{
    double samplerate = 44100;
    int err = mpg123_init();
    mpg123_handle* handle = NULL;
    handle = mpg123_new(NULL, &err);    
    err = mpg123_open_feed(handle);
    err = mpg123_param(handle, MPG123_REMOVE_FLAGS, MPG123_GAPLESS, 0);
    mpg123_format_none(handle);
    err = mpg123_format(handle, samplerate, channels, MPG123_ENC_FLOAT_32);
    
    
    Mp3Parser parser = getParser(inData);
    
    int f = 0;
    bool readOK = true;
    bool nextFrameOK = true;
    do {
        Mp3Frame frame = parser.GetCurrentFrame();
        bool isValid = frame.IsValid();
        bool isXing = frame.GetHeader().mIsXing;
        if (isValid && !isXing)
        {
            if (nbSkipInFrames > 0)
            {
                nbSkipInFrames--;
            }
            else 
            {
                int byteLength = frame.GetByteLength();
                std::vector<uint8> frameData;
                frameData.resize(byteLength);
                readOK = parser.ReadFrameBytes(frameData);
                err = mpg123_feed(handle, &frameData[0], byteLength);
                NSLog(@"feed (%d bytes)  res = %s", byteLength, mpg123_plain_strerror(err));
                f++;
            }
            
        }
        nextFrameOK = parser.GoToNextFrame();
    } while (f < nbInFrames && nextFrameOK && readOK);
    
    NSMutableData* outData = [NSMutableData data];
    size_t outSize = 1152 * channels * sizeof(float);
    unsigned char outBuffer[outSize];
    size_t outDone = 0;
    do {
        err = mpg123_read(handle, outBuffer, outSize, &outDone);
        [outData appendBytes:outBuffer length:outDone];
        NSLog(@"read (ask = %d  done = %d)  res = %s", (int)outSize, (int)outDone, mpg123_plain_strerror(err));
    } while (err == MPG123_OK || err == MPG123_NEW_FORMAT);
    
    
    if (skipDecoderDelay)
    {
//        int skip = 1104 * channels * sizeof(float);
        int skip = 528 * channels * sizeof(float);
        const char* bytes = (const char*)outData.bytes;
        int nbBytes = outData.length;
        nbBytes -= skip;
        bytes = bytes + skip;
        if (nbBytes < 0)
            nbBytes = 0;
        outData = [NSMutableData dataWithBytes:bytes length:nbBytes];
    }
    
    
    return outData;
}

bool decodeAndWriteToFile(NSData* inData, int nbInFrames, int nbSkipInFrames, NSString* destFileName, bool skipDecoderDelay, int channels)
{
    NSData* data = decode(inData, nbInFrames, nbSkipInFrames, skipDecoderDelay, channels);
    writeToFile(destFileName, data);
    return true;
}


NSData* decode(NSString* srcPath, int nbInFrames, int nbSkipInFrames, bool skipDecoderDelay, int channels)
{
    NSData* inData = [NSData dataWithContentsOfFile:srcPath];
    NSData* outData = decode(inData, nbInFrames, nbSkipInFrames, channels);
    return outData;
}

void writeToFile(NSString* destFileName, NSData* data)
{
    NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDesktopDirectory, NSUserDomainMask, NO);
    NSString* fullPathToDesktop = [paths objectAtIndex:0];
    NSString* outpath = [[fullPathToDesktop stringByAppendingPathComponent:destFileName] stringByExpandingTildeInPath];
    [data writeToFile:outpath atomically:YES];
}

bool decodeAndWriteToFile(NSString* srcPath, int nbInFrames, int nbSkipInFrames, NSString* destFileName, bool skipDecoderDelay, int channels)
{
    NSData* data = decode(srcPath, nbInFrames, nbSkipInFrames, skipDecoderDelay, channels);
    writeToFile(destFileName, data);
    return true;
}

Mp3Parser getParser(NSData* data)
{
    NSUInteger inByteCount = data.length;
    const char* inBytes = (const char*)data.bytes;
    
    nglIMemory* pInStream = new nglIMemory(inBytes, inByteCount); // memory leak !!!!!! (just for testing)
    Mp3Parser parser(*pInStream, false, true);
    return parser;
}



void OnLameError(const char *format, va_list ap)
{
  //  nglString err;
  //  err.CFormat(format, ap);
  //  NGL_LOG("Player", NGL_LOG_INFO, "Lame error: %s\n", err.GetChars()); 
  vprintf(format, ap); 
}

void OnLameDebug(const char *format, va_list ap)
{
  //  nglString err;
  //  err.CFormat(format, ap);
  //  NGL_LOG("Player", NGL_LOG_INFO, "Lame debug: %s\n", err.GetChars()); 
  vprintf(format, ap); 
}

void OnLameMsg(const char *format, va_list ap)
{
  //  nglString err;
  //  err.CFormat(format, ap);
  //  NGL_LOG("Player", NGL_LOG_INFO, "Lame message: %s\n", err.GetChars()); 
  vprintf(format, ap); 
}


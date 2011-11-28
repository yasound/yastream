//
//  main.m
//  testMp3Parser
//
//  Created by matthieu campion on 11/23/11.
//  Copyright (c) 2011 MXP4. All rights reserved.
//

#import <Foundation/Foundation.h>
#include "MP3Parser.h"
#include <time.h>

int main (int argc, const char * argv[])
{

  @autoreleasepool 
  {
    if (argc  < 2)
      return -1;
    
//    NSString* file = [NSString stringWithCString:argv[1] encoding:NSUTF8StringEncoding];
    NSString* file = @"/Users/mat/Desktop/src.mp3";
    NSData* data = [NSData dataWithContentsOfFile:file];
    if (!data)
      return -2;
    
    unsigned char* bytes = (unsigned char*)data.bytes;
    int nbBytes = (int)data.length;
    
    Mp3Parser parser(bytes, nbBytes);
    
    int count = 1000;
    NSTimeInterval total = 0;
    for (int i = 0; i < count; i++)
    {
      NSDate* start = [NSDate date];
      parser.ParseAll();
      NSTimeInterval elapsed = -[start timeIntervalSinceNow];
      //      NSLog(@"parsing duration %d: %lf", i, elapsed);
      
      total += elapsed;
    }
    
    NSTimeInterval mean = total / count;
    NSLog(@"parsing mean duration: %lf", mean);
      
  }
    return 0;
}


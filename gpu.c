#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "header.h"
#include "memory.h"
#include "control.h"
#include "Debugging.h"
#include "gpu.h"

 //every dot and their hue

struct LCD lcd;
int ScanlineCounter = 456; //456 cycles per scan line, there is a 146 vblank buffer
unsigned char DisplayPixels [256 * 256 * 3];
unsigned char SpriteExplorerDisplay [256 * 256 * 3];

unsigned char* GetPixelColor (unsigned char bit1, unsigned char bit2, int pallette);

unsigned short GetBGMAPAddress (void) 
{
    unsigned short address = (lcd.control & 0x08) ? 0x9C00 : 0x9800;
    return address;
}

unsigned short GetDataAddress (void) 
{
    unsigned short address = (lcd.control & 0x10) ? 0x8000 : 0x8800;
    return address;
}

unsigned char UsingWindow (void)
{
    return (lcd.control & 0x20);
}

unsigned short GetWINMAPAddress(void) 
{
    unsigned short address = (lcd.control & 0x40) ? 0x9C00 : 0x9800;
    return address;
}

void UpdateGraphics (int cycles) {

    SetLCDstatus();

    if(IsLcdOn()) {
        ScanlineCounter -= (cycles * 4);
    } else {
        return;
    }
    
    if (ScanlineCounter <= 0) {

        
        int overflow = (-1 * ScanlineCounter);
        ScanlineCounter = 456;
        ScanlineCounter -= overflow;
        lcd.LY++;
        

        if(lcd.LY == 144) {
            
            RequestInterrupt(0);
            

        } else if (lcd.LY > 153) {
            
            lcd.LY = 0; //reset if outside range
            DrawScanline();


        } else if (lcd.LY < 144) {
            
            
            DrawScanline(); //this will add a line to the render buffer


        }

    } 

}

int IsLcdOn (void) {
    return (lcd.control & 0x80);
}

void SetLCDstatus (void) {
    //leave the 7th bit
    if (!IsLcdOn()) {

        //lcd off
        ScanlineCounter = 456;
        lcd.LY = 0;
        lcd.status &= 0xFC; //reset mode to 1 Vblank when off
        lcd.status |= 0x01;
        return;

    } 

    unsigned char currentMode = (lcd.status & 0x03);

    unsigned char mode = 0;
    unsigned char ReqInt = 0;

    if (lcd.LY >= 144) {
        
        mode = 1;
        lcd.status &= 0xFC;
        lcd.status |= 0x01;
        ReqInt = (lcd.status & 0x10); // bit 4

    } 
    
    if (ScanlineCounter){ //to buffer in the beginning 

        int mode2bounds = 456-80 ;
        int mode3bounds = mode2bounds - 172 ;

        if(ScanlineCounter >= mode2bounds ) {

            mode = 2;
            lcd.status &= 0xFC;
            lcd.status |= 0x02; //searching sprite attributes
            ReqInt = (lcd.status & 0x20); //bit 5

        } else if (ScanlineCounter >= mode3bounds) {

            mode = 3;
            lcd.status &= 0xFC; //transfering data to LCD driver
            lcd.status |= 0x03;
            

        } else {

            mode = 0;
            lcd.status &= 0xFC;
            ReqInt = (lcd.status & 0x08); //Hblank

        }


        if (ReqInt && (currentMode != mode)) {
            RequestInterrupt(1); //stat interrupt
        }

        if (lcd.LY == lcd.LYC) {
            
            lcd.status |= 0x04;

            if(lcd.status & 0x40) { //bit 6
                RequestInterrupt(1);
            }

        } else {
            lcd.status &= ~(0x04); //everything but second bit
        }


    }

}

void DrawScanline(void) {

    if(lcd.control & 0x80) {
        
        LoadLineFromMap();
        //LoadTilesFromMap();

    } 
    
    if (lcd.control & 0x02) {

        LoadSpritesOnScreen();
        //LoadSpriteLine();

    }
}

void LoadSpritesOnScreen (void) {
    //int UnderBGWIN = lcd.control & 0x01;
    unsigned char SpriteYPos = 0;
    unsigned char SpriteXpos = 0;
    unsigned short DataAddress = 0x8000; //sprites stored 0x8000 - 0x8FFF;
    unsigned char SpriteHeight = (lcd.control & 0x04) ? 16 : 8; //double or not
    int spritesloaded = 0;
    //works but doesn't have attributes nor proper priority;

    for (int SpritetoLoad = 0; SpritetoLoad < 40; SpritetoLoad++) 
    {
        //if(SpritetoLoad == 0) printf("Sprite: %d, ID: %02X, Xcoord: %d, Ycoord: %d Attribute: %02X\n", SpritetoLoad, ReadByte(0xFE00 + SpritetoLoad * 4 + 2), ReadByte(0xFE00 + SpritetoLoad * 4 + 1), ReadByte(0xFE00 + SpritetoLoad * 4 ), ReadByte(0xFE00 + SpritetoLoad * 4 +3));
        
        SpriteYPos = ReadByte(0xFE00 + SpritetoLoad * 4);
        if(SpriteYPos == 0) continue;

        SpriteXpos = ReadByte(0xFE00 + SpritetoLoad * 4 + 1);
        
        int SpritePixelOffset = (SpriteYPos > 15) ? (SpriteXpos - 8) * 3 + (SpriteYPos - 16) * 256 * 3 : (SpriteXpos - 8) * 3;

        unsigned short SpriteDataAddress = DataAddress;
        
        int offset = ReadByte(0xFE00 + SpritetoLoad * 4 + 2);
        SpriteDataAddress += (offset * 16);

        int SpriteStartingLine = (SpriteYPos > 15) ? 0 : -1 * (SpriteYPos - 16); //what line of sprite do i draw first
        
        //Attributes
        int SpriteVMirror = ReadByte(0xFE00 + SpritetoLoad * 4 + 3) & 0x40;
        int SpriteHMirror = ReadByte(0xFE00 + SpritetoLoad * 4 + 3) & 0x20;
        int SpritePallette = ReadByte(0xFE00 + SpritetoLoad * 4 + 3) & 0x10;
        //BG override
        SpritePallette = (ReadByte(0xFE00 + SpritetoLoad * 4 + 3) & 0x80) ? 2: SpritePallette;

        for(int lineofSprite = SpriteStartingLine; lineofSprite < SpriteHeight; lineofSprite++)
        {
            unsigned char byte1;
            unsigned char byte2;

            int lineoffset = (lineofSprite) * (256) * 3;// should start printing only @lcd.LY independent of positon of lcd.SCY

            if(SpriteVMirror){
                    
                    byte1 = ReadByte( SpriteDataAddress + (SpriteHeight - lineofSprite) * 2); //off set by their position in the tile
                    byte2 = ReadByte( SpriteDataAddress + (SpriteHeight - lineofSprite) * 2 + 1);

            } else {

                    byte1 = ReadByte( SpriteDataAddress + lineofSprite * 2); //off set by their position in the tile
                    byte2 = ReadByte( SpriteDataAddress + lineofSprite * 2 + 1);

            }

            for(int pixel = 0; pixel < 8; pixel++)
            {
                unsigned char bit1;
                unsigned char bit2;

                int PixelOffset = pixel * 3;

                if(SpriteHMirror){
                    
                    bit1 = (byte1 & (0x01 << pixel));
                    bit2 = (byte2 & (0x01 << pixel));

                } else {

                    bit1 = (byte1 & (0x80 >> pixel));
                    bit2 = (byte2 & (0x80 >> pixel));

                }


                unsigned char *colors;
                colors = GetPixelColor(bit1, bit2, SpritePallette);

                //00 means transparent, so no write
                if(bit1 || bit2){

                    DisplayPixels[SpritePixelOffset + lineoffset + PixelOffset] = colors[0];
                    DisplayPixels[SpritePixelOffset + lineoffset + PixelOffset + 1] = colors[1];
                    DisplayPixels[SpritePixelOffset + lineoffset + PixelOffset + 2] = colors[2];

                }
            }
        }
        
    }

}

void LoadSpriteLine (void) {
    //int UnderBGWIN = lcd.control & 0x01;

    unsigned short DataAddress = 0x8000; //sprites stored 0x8000 - 0x8FFF;

    int spritecounter = 0;
    unsigned char ValidSprites[10];
    unsigned char SpriteHeight = (lcd.control & 0x04) ? 16 : 8; //double or not

    //memset(ValidSprites, 0, 10);

    for (int SpriteNum = 0; SpriteNum < 40; SpriteNum++)
    {
        //not reading directly from OAM yet, for whatever reason, i'll figure it out
        unsigned char SpriteYPos = ReadByte(0xFE00 + ( SpriteNum * 4 )); //top is this -16
        //is the sprite on LY
        if(lcd.LY >= (SpriteYPos - 16) && lcd.LY < (SpriteYPos - 16 + SpriteHeight)) 
        {
            ValidSprites[spritecounter] = SpriteNum;
            spritecounter++;
            
        }

        if(spritecounter == 10) break;

    }
    
    unsigned char Xpositions[10];
    //memset(Xpositions, 0, 10);
    //populate Xpositions
    for (int sprite = 0; sprite < 10; sprite++)
    {   
        Xpositions[sprite] = ReadByte(0xFE00 + ValidSprites[sprite] * 4 + 1);
    }

    unsigned char SortedSprites[10];
    //memset(SortedSprites, 0, 10);
    //sort the sprites in terms of which will be drawn first so that overlap functions correctly
    for (int times = 0; times < 10; times++)
    {
        int RightMostSprite = 0;
        int HighestX = -1;
        
        for (int sprites = 0; sprites < 10; sprites++){

            int CurrXpos = Xpositions[sprites];

            if((CurrXpos > HighestX)) 
            {
                HighestX = CurrXpos;
                RightMostSprite = sprites;
            }
            
        }

        SortedSprites[times] = RightMostSprite;
        Xpositions[RightMostSprite] = -1; //clears that sprite from being chosen again.
    }

    int spritesloaded = 0;
    for (int SpritetoLoad = 0; SpritetoLoad < 10; SpritetoLoad++) 
    {

        //alter to fix filtering 
        unsigned char SpriteYPos = ReadByte(0xFE00 + SortedSprites[SpritetoLoad] * 4);
        //if(!(lcd.LY >= (SpriteYPos - 16) && lcd.LY < (SpriteYPos - 16 + SpriteHeight))) continue;

        unsigned char SpriteXpos = ReadByte(0xFE00 + SortedSprites[SpritetoLoad] * 4 + 1);
        
        int SpritePixelOffset = (SpriteXpos - 8) * 3;

        unsigned short SpriteDataAddress = DataAddress;
        
        int offset = ReadByte(0xFE00 + SortedSprites[SpritetoLoad] * 4 + 2);
        SpriteDataAddress += (offset * 16);
        
        int lineofSprite = (lcd.LY - (SpriteYPos - 16)); //check functionality across sizes

        int lineoffset = (lcd.LY) * (256) * 3;// should start printing only @lcd.LY independent of positon of lcd.SCY

        unsigned char byte1 = ReadByte( SpriteDataAddress + lineofSprite * 2); //off set by their position in the tile
        unsigned char byte2 = ReadByte( SpriteDataAddress + lineofSprite * 2 + 1);

        for(int pixel = 0; pixel < 8; pixel++)
        {
            
            int PixelOffset = pixel * 3;

            unsigned char bit1 = (byte1 & (0x80 >> pixel));
            unsigned char bit2 = (byte2 & (0x80 >> pixel));

            unsigned char *colors;
            colors = GetPixelColor(bit1, bit2, 0);

            //00 means transparent, so no write
            if(bit1 || bit2){

                DisplayPixels[SpritePixelOffset + lineoffset + PixelOffset] = colors[0];
                DisplayPixels[SpritePixelOffset + lineoffset + PixelOffset + 1] = colors[1];
                DisplayPixels[SpritePixelOffset + lineoffset + PixelOffset + 2] = colors[2];

            }
            
            
        }
        
    }

}

unsigned char * LoadNintendoLogo (void){

    static unsigned char PixelBatch[6 * 8 * 8 * 3];

    for (int tile = 0; tile < 48; tile++ )
    {
        int tileOffset = (tile % 2) ? ((tile % 2) * 48 * 3 * 2 + (tile/2 * 4 * 3)) : (tile/2 * 4 * 3); // if odd, skip two lines
        
        tileOffset += (tile >= 24) ? 48 * 3 * 3 : 0; //next line
        unsigned char byte = ReadByte(0x0104 + tile);

        for (int i = 0; i < 8; i++)
        {

        int offset = (i > 3) ? i * 3 + (44 * 3) : i * 3;
        unsigned char color = (byte & (0x80>>i)) ? 0x0 : 0xFF;

        DisplayPixels[offset + tileOffset] = color;
        DisplayPixels[offset + tileOffset + 1] = color;
        DisplayPixels[offset + tileOffset + 2] = color;
        
        }
    }

    return PixelBatch;
}

void PrintOAM (void) 
{

    for (int i = 0; i < 40; i++)
    {
        if(ReadByte(0xFE00 + i * 4 + 2) == 0) continue;
        printf("OAM[%i]: %02X %02X %02X %02X | ", i, 
        ReadByte(0xFE00 + i * 4), ReadByte(0xFE00 + i * 4 + 1), 
        ReadByte(0xFE00 + i * 4 + 2), ReadByte(0xFE00 + i * 4 + 3));

        if(!(i % 2)) printf("\n");
    }

}

void LoadOAM (void) {

    //PrintOAM();

    unsigned short DataAddress = 0x8000;
    int SpriteLength = (lcd.control & 0x04) ? 16 : 8; 

    for (int SpritetoLoad = 0; SpritetoLoad < 40; SpritetoLoad++) 
    {
        //sprite should be spaced 
        int SpritePixelOffset =  8 * 3 + 256 * 3 * 10 + ( (SpritetoLoad / 4) * 256 * 15 + (SpritetoLoad % 4) * 20) * 3;
        int offset = ReadByte(0xFE00 + SpritetoLoad * 4 + 2);
        unsigned short SpriteDataAddress = DataAddress + (offset * 16);
        
        int SpritePallette = ReadByte(0xFE00 + SpritetoLoad * 4 + 3) & 0x10;

        for(int lineofSprite = 0; lineofSprite < SpriteLength; lineofSprite++)
        {
            int lineoffset = lineofSprite * 256 * 3; //move to beginning of line
            unsigned char byte1 = ReadByte( SpriteDataAddress + lineofSprite * 2); //off set by their position in the tile
            unsigned char byte2 = ReadByte( SpriteDataAddress + lineofSprite * 2 + 1);

            for(int pixel = 0; pixel < 8; pixel++)
            {
                
                int PixelOffset = pixel * 3;

                unsigned char bit1 = (byte1 & (0x80 >> pixel));
                unsigned char bit2 = (byte2 & (0x80 >> pixel));

                unsigned char *colors;
                colors =  GetPixelColor(bit1, bit2, SpritePallette);

                //00 means transparent, so no write
                if(bit1 || bit2){

                    SpriteExplorerDisplay[SpritePixelOffset + lineoffset + PixelOffset] = colors[0];
                    SpriteExplorerDisplay[SpritePixelOffset + lineoffset + PixelOffset + 1] = colors[1];
                    SpriteExplorerDisplay[SpritePixelOffset + lineoffset + PixelOffset + 2] = colors[2];

                }
            
            
            }
        }
        
    }
}

void LoadSpriteSheet (void)
{   
    //displays all tiles currently mapped in the BG, doesn't hold scanline functionality but makes sense. 
    //returns 8 x 8 pixel bitmap
    //unsigned only 0x8000-

    unsigned short DataAddress = 0x8000;
    
    for (int tile = 0; tile < ( 32 * 12 ); tile++)
    {
        int tileOffset = (tile % 16) * 8 * 3 + ((tile / 16) * 256 * 3 * 8); //row then coloumn
        //if(tile % 32)
        for (int line = 0; line < 8; line++)
        {
            int lineoffset =  line * (256) * 3; 

            unsigned char byte1 = ReadByte( DataAddress + line * 2 ); //off set by their position in the tile
            unsigned char byte2 = ReadByte( DataAddress + line * 2 + 1 );

            
            for(int pixel = 0; pixel < 8; pixel++)
            {
                
                int PixelOffset = pixel * 3;

                unsigned char bit1 = (byte1 & (0x80 >> pixel));
                unsigned char bit2 = (byte2 & (0x80 >> pixel));

                unsigned char *colors;
                colors = GetPixelColor(bit1, bit2, 2);

                SpriteExplorerDisplay[tileOffset + lineoffset + PixelOffset] = colors[0];
                SpriteExplorerDisplay[tileOffset + lineoffset + PixelOffset + 1] = colors[1];
                SpriteExplorerDisplay[tileOffset + lineoffset + PixelOffset + 2] = colors[2];
  
                
            }
        }

        DataAddress += 16; //each tile move forward 16 bytes
    }


}

void LoadTilesFromMap (void)
{   

    //displays all tiles currently mapped in the BG, doesn't hold scanline functionality but makes sense. 
    //returns 8 x 8 pixel bitmap
    //unsigned only 0x8000-
    unsigned short MapAddress = (lcd.control) ? GetBGMAPAddress() : 0x9800; 
    unsigned short DataAddress = (lcd.control) ? GetDataAddress() : 0x8000;
    
    unsigned short TileIDAddress = MapAddress;
    //BG
    
    for (int tile = 0; tile < ( 32 * 32 ); tile++)
    {
        
        int tileOffset = (tile % 32) * 8 * 3 + ((tile / 32) * 256 * 3 * 8); //row then coloumn

        int TileID = (DataAddress == 0x8000) ? (unsigned char) ReadByte(TileIDAddress + tile) : (signed char) ReadByte(TileIDAddress + tile);
        unsigned short TileDataAddress = DataAddress;

        int offset = (DataAddress == 0x8000) ? (TileID * 16) : ((TileID + 128) * 16);
        TileDataAddress += (offset); 

        for (int line = 0; line < 8; line++)
        {
            int lineoffset = line * (256) * 3;// moves back to first spot

            unsigned char byte1 = ReadByte( TileDataAddress + line * 2); //off set by their position in the tile
            unsigned char byte2 = ReadByte( TileDataAddress + line * 2 + 1 );

            // 
            for(int pixel = 0; pixel < 8; pixel++)
            {
                
                int PixelOffset = pixel * 3;

                unsigned char bit1 = (byte1 & (0x80 >> pixel));
                unsigned char bit2 = (byte2 & (0x80 >> pixel));

                unsigned char *colors;
                colors = GetPixelColor(bit1, bit2, 2);

                SpriteExplorerDisplay[tileOffset + lineoffset + PixelOffset] = colors[0];
                SpriteExplorerDisplay[tileOffset + lineoffset + PixelOffset + 1] = colors[1];
                SpriteExplorerDisplay[tileOffset + lineoffset + PixelOffset + 2] = colors[2];
    
                
            }
        }
    }


}

void LoadLineFromMap (void)
{
    //scrolls automatically to fit everything within 160 * 144 in display
    //saves from having to dynamically asdjust the src and dst rect which would case distortion
    
    unsigned short DataAddress = (lcd.control) ? GetDataAddress() : 0x8000;
    unsigned short TileIDAddress = (lcd.control) ? GetBGMAPAddress() : 0x9800;


    int Tileshift = (lcd.SCX/8);

    int firstTileInLine = ((((unsigned char)lcd.LY + lcd.SCY) / 8) * 32);    
    int lastTileInLine = (((unsigned char)(lcd.LY + lcd.SCY )/ 8) * 32) + 31; // char so it scrolls

    int firstTiletoWrite = (((unsigned char)(lcd.LY + lcd.SCY) / 8) * 32) + Tileshift;
    
    for (int tile = firstTiletoWrite; tile < firstTiletoWrite + 20; tile++) //dont do 32 tiles after
    {

        tile = (tile > lastTileInLine) ? firstTileInLine + (tile - lastTileInLine) : tile;
        
        int tileOffset = ((tile - firstTiletoWrite) % 32) * 8 * 3; //row then coloumn

        //if using window, check tile
        int WindowStartingTile = ( (lcd.WX - 7) / 8);
        // printf("windowstartingtile: %d", WindowStartingTile);

        int Windowtileshift = 0;
        if(UsingWindow() && (lcd.LY >= lcd.WY) && ((tile % 32) >= WindowStartingTile)) //((tile % 32) >= WindowStartingTile)
        {

            TileIDAddress = GetWINMAPAddress();
            Windowtileshift = ((lcd.WY - lcd.SCY)/8) * 32; //number of lines because WY starts the window at the beginning
        }

        int TileID = (DataAddress == 0x8000) ? (unsigned char) ReadByte(TileIDAddress + tile - Windowtileshift) : (signed char) ReadByte(TileIDAddress + tile - Windowtileshift);
        int offset = (DataAddress == 0x8000) ? (TileID * 16) : ((TileID + 128) * 16);
        
        unsigned short TileDataAddress = DataAddress;
        TileDataAddress += (offset); 

        int line = (lcd.LY + lcd.SCY) % 8;
        int lineoffset = (lcd.LY) * (256) * 3;// should start printing only @lcd.LY independent of positon of lcd.SCY

        unsigned char byte1 = ReadByte( TileDataAddress + line * 2); //off set by their position in the tile
        unsigned char byte2 = ReadByte( TileDataAddress + line * 2 + 1 );     

        for(int pixel = 0; pixel < 8; pixel++)
        {
            
            int PixelOffset = pixel * 3;

            unsigned char bit1 = (byte1 & (0x80 >> pixel));
            unsigned char bit2 = (byte2 & (0x80 >> pixel));

            unsigned char *colors;
            colors = GetPixelColor(bit1, bit2, 2);
            if(!(lcd.control & 0x01)) colors = GetPixelColor(0, 0, 2);

            DisplayPixels[tileOffset + lineoffset + PixelOffset] = colors[0];
            DisplayPixels[tileOffset + lineoffset + PixelOffset + 1] = colors[1];
            DisplayPixels[tileOffset + lineoffset + PixelOffset + 2] = colors[2];
            
        }
        
    }
}

unsigned char * GetPixelColor (unsigned char bit1, unsigned char bit2, int pallette)
{

    struct Pallette {

        union{
            
            unsigned char data;
            struct {

            unsigned char index0 : 2;
            unsigned char index1 : 2;
            unsigned char index2 : 2;
            unsigned char index3 : 2;

            };
        };
        

    };

    struct Pallette colorway;
    switch (pallette)
    {
    case 2: //BG
        colorway.data = ReadByte(0xFF47);
        break;
    case 1:
        colorway.data = ReadByte(0xFF49);
        break;
    case 0:
        colorway.data = ReadByte(0xFF48);
        break;
    default:
        break;
    }

    static unsigned char Colors[12] = {

        //white, light, dark, black
        0xE0, 0xF8, 0xD0,
        0x88, 0xC0, 0x70,
        0x34, 0x68, 0x56,
        0x08, 0x18, 0x20

    };

    //{0xE0, 0xF8, 0xD0}
    if (bit1 && bit2) 
    {
        //index 3
        
        return &Colors[colorway.index3 * 3]; 
    } else if (bit2) {

        //index 2
        return &Colors[colorway.index2 * 3];  

    } else if (bit1) {

        //index 1
        return &Colors[colorway.index1 * 3];  

    } else {

        //index 0
        return &Colors[colorway.index0 * 3];  

    } 

    
}
    
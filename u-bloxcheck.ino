//*********************************************************************************
// U-Blox Check - Boron x404
// Whatch U-Blox celluar modem connect (or fail)
//
// written by:  Terje B. Nilsen, Spring Texas
// version:     1.2
// date:        14 JUL 2023
//
//*********************************************************************************

//To change SIM usage, uncomment one of the below defines
//Burn code into Boron via DFU (blinking yellow mode and USB cable)
//Wait for D7 LED to go blue
//Comment the blow lines out again
//Burn code via DFU mode and now SIM will be used

//#define CHANGE_SIM_TO_EXTERNAL 1
//#define CHANGE_SIM_TO_INTERNAL 1


SYSTEM_MODE(SEMI_AUTOMATIC); //We handle modem power and connection
//SYSTEM_THREAD(ENABLED);

#define BANDMASK_UK "185481375"  //All valid LTE bands for the u-blox SARA-R510S-01B modem 1,2,3,4,5,8,12,13,14,18,19,20,25,26,28

int AT_cmd;

#define NATCOMMANDS 24
char AT_commands[NATCOMMANDS][32] = {
    "AT+CCID\r\n",//0
    "AT+UMNOPROF?\r\n",//1
    "AT+URAT?\r\n",//2
    "AT+COPS?\r\n",//3
    "AT+CEREG?\r\n",//4
    "AT+UBANDMASK?\r\n",//5
    "AT+UBANDMASK=?\r\n",//6
    "AT+UMNOCONF?\r\n",//7
    "AT+CGDCONT?\r\n",//8
    "AT+CGMI\r\n",//9
    "AT+CGMM\r\n",//10
    "AT+CGMR\r\n",//11
    "AT+CGSN\r\n",//12
    "AT+CIMI\r\n",//13
    "AT+CIND?\r\n",//14
    "AT+CCLK?\r\n",//15
    "AT+CNUM\r\n",//16
    "AT+CSQ\r\n",//17
    "AT+CESQ\r\n",//18
    "AT+UDOPN=2\r\n",//19
    "ATI0\r\n",//20
    "ATI7\r\n",//21
    "ATI9\r\n",//22
    "AT+UGPIOR=23\r\n"//23
};

//****************
//Modem Parameters 
//****************
String sCcid;
String sMno;
String sRat;
String sBandmask;
String sCdg;
String sApn;
String sIp;
String sMfg;
String sModel;
String sImei;
String sImsi;
String sHw;
String sFw;
String sClk;
String sSq;
String sCell;
String sStrength;
String sQuality;
String sSim;

unsigned long int last_read;

//*****************************************
//Holds returned string response from modem
//*****************************************
char at_response[2050];

//*******************************
//Start the serial debug messages
//*******************************
SerialLogHandler logHandler(LOG_LEVEL_ALL);

//*************
//Setup routine
//*************
void setup()
{
    //**************************************************
    //Deal with SIM card change - External 3rd Party SIM
    //**************************************************
    #if (CHANGE_SIM_TO_EXTERNAL == 1)
    Cellular.setActiveSim(EXTERNAL_SIM);
    Cellular.setCredentials("iot.t-mobile.nl"); //Set to Orkney APN
    #endif

    //*************************************************
    //Deal with SIM card change - Internal Particle SIM
    //*************************************************
    #if (CHANGE_SIM_TO_INTERNAL == 1)
    Cellular.setActiveSim(INTERNAL_SIM);
    Cellular.clearCredentials();
    #endif
    
    //************************
    //Turn on onboard blue LED
    //************************
    pinMode(D7, OUTPUT);
    digitalWrite(D7, HIGH);

    #if (CHANGE_SIM_TO_EXTERNAL == 1 || CHANGE_SIM_TO_INTERNAL == 1)
    return; //Just exit if we are changeing SIM
    #endif
    
    //*******************
    //Map Cloud Variables
    //*******************
    Particle.variable("CCID",   &sCcid,     STRING);
    Particle.variable("IMEI",   &sImei,     STRING);
    Particle.variable("IMSI",   &sImsi,     STRING);
    Particle.variable("MNO",    &sMno,      STRING);
    Particle.variable("RAT",    &sRat,      STRING);
    Particle.variable("BAND",   &sBandmask, STRING);

    Particle.variable("CDG",    &sCdg,      STRING);
    Particle.variable("APN",    &sApn,      STRING);
    Particle.variable("IP",     &sIp,       STRING);
    Particle.variable("Signal", &sSq,       STRING);

    Particle.variable("MFG",    &sMfg,      STRING);
    Particle.variable("MODEL",  &sModel,    STRING);
    Particle.variable("HWREV",  &sHw,       STRING);
    Particle.variable("FWREV",  &sFw,       STRING);
    Particle.variable("SIM",    &sSim,      STRING);

    Particle.variable("MClock", &sClk,      STRING);
    Particle.variable("Service",&sCell,     STRING);
    
    Particle.variable("Strength",&sStrength,STRING);
    Particle.variable("Quality",&sQuality,STRING);
    
    delay(6000); //Wait for our PC to start the serial monitor
    Log.info("*** U-Blox Check START ***\r\n");

    //**************************************************
    //Turn on the modem and start the connection process
    //**************************************************
    Cellular.on(); 
    Cellular.connect();
}
//***************
//Background loop
//***************
void loop()
{
    static int keep_alive_set = 0;

    #if (CHANGE_SIM_TO_EXTERNAL == 1 || CHANGE_SIM_TO_INTERNAL == 1)
    return; //Just exit if we are changeing SIM
    #endif
    
    //***********************************************
    //Make sure we're connected to the Particle Cloud
    //***********************************************
    if (Particle.connected())
    {
        if ((millis() - last_read) > 5000)
        {
            DoATCommands(0);
            last_read = millis();
        }
    }
    else
    {
        //************************************
        //See if modem is connected to a tower
        //************************************
        if (Cellular.ready())
        {
            DoATCommands(1);
            Log.info("*** Cellular Connection Established (%s) ***\r\n",sCell.c_str());
            delay(2000); //Pause so we can see it on Serial Monitor
            //*************************
            //Connect to Particle Cloud
            //*************************
            Particle.connect();
            Log.info("*** Connect to Particle Cloud ***\r\n");
        }
        //*****************************
        //Check modem connection status
        //*****************************
        if ((millis() - last_read) > 1000)
        {
            Cellular.command(callbackAT, at_response, 10000, AT_commands[4]);
            last_read = millis();
        }
    }
    //************************************************************
    //Check to see if we need to set the keep alive time (seconds)
    //************************************************************
    if (keep_alive_set == 0 && sMno == "90") //Only set if we're in Blighty
    {
        keep_alive_set = 1;
        Particle.keepAlive(120);
    }
}
//**********************************
//Execute all configured AT commands
//**********************************
void DoATCommands(int skip_delay)
{
    //*******************************************************
    //Loop thru list of commands to extract modem information
    //*******************************************************
    for (AT_cmd = 0; AT_cmd < NATCOMMANDS; AT_cmd++)
    {
        Cellular.command(callbackAT, at_response, 10000, AT_commands[AT_cmd]);
        if (!skip_delay)
            delay(1000);
    }
    //********************************
    //Get pre-calculated signal values
    //********************************
    CellularSignal sig = Cellular.RSSI();
    sStrength = String::format("%.02f%%",sig.getStrength());
    sQuality  = String::format("%.02f%%",sig.getQuality());
}
//*******************
//Receive AT response
//*******************
int callbackAT(int type, const char* buf, int len, char* rets)
{
    const char s[2] = ",";
    char sret[2048];
    char *token;
    unsigned long int bmask,mask;
    int i;
    
    //**********
    //Debug info
    //**********
    if (buf && strlen(buf) && TYPE_OK != type)
    {
        Log.info("%s\r\n", buf);
    }
    //********************************
    //Parse parameter(s) from response
    //********************************
    if ((type != TYPE_OK) && rets)
    {
        //***********************************
        //Parse response according to command
        //***********************************
        switch(AT_cmd)
        {
            case 0:
                sscanf(buf, "\r\n+CCID: %[^\r]\r\n", rets);
                sCcid = rets;
                break;
            case 1:
                sscanf(buf, "\r\n+UMNOPROF: %[^\r]\r\n", rets);
                sMno = rets;
                break;
            case 2:
                sscanf(buf, "\r\n+URAT: %[^\r]\r\n", rets);
                sRat = rets;
                if (sRat == "7")
                {
                    sRat = "LTE-M";
                }
                else if (sRat == "8")
                {
                    sRat = "NB-IoT";
                }
                break;
            case 5:
                sscanf(buf, "\r\n+UBANDMASK: %[^\r]\r\n", rets);
                token = strtok(rets, s);
                token = strtok(NULL, s);
                sBandmask = token;
                sscanf(sBandmask.c_str(), "%d", &bmask);
                sBandmask = "";
                i = 1;
                mask = 1;
                while(i<33)
                {
                    if (bmask & mask)
                    {
                        if (sBandmask != "")
                        {
                            sBandmask += ",";
                        }
                        sprintf(rets,"%d",i);
                        sBandmask += rets;
                    }
                    mask <<= 1;
                    i++;
                }
                break;
            case 8:
                sscanf(buf, "\r\n+CGDCONT: 1,%[^\r]\r\n", rets);
                sCdg = rets;
                sCdg.replace("\"", "");
                strcpy(sret,sCdg.c_str());
                token = strtok(sret, s);
                token = strtok(NULL, s);
                sApn = token;
                token = strtok(NULL, s);
                sIp = token;
                break;
            case 9:
                sscanf(buf, "\r\n %[^\r]\r\n", rets);
                sMfg = rets;
                break;
            case 10:
                sscanf(buf, "\r\n %[^\r]\r\n", rets);
                //sModel = rets;
                break;
            case 11:
                sscanf(buf, "\r\n %[^\r]\r\n", rets);
                //sFw = rets;
                break;
            case 12:
                sscanf(buf, "\r\n %[^\r]\r\n", rets);
                sImei = rets;
            case 13:
                sscanf(buf, "\r\n %[^\r]\r\n", rets);
                sImsi = rets;
                break;
            case 15:
                sscanf(buf, "\r\n+CCLK: ""%[^\r]\r\n""", rets);
                sClk = rets;
                sClk.replace("\"", "");
                break;
            case 17:
                sscanf(buf, "\r\n+CSQ: ""%[^\r]\r\n""", rets);
                sSq = rets;
                break;
            case 19:
                sscanf(buf, "\r\n+UDOPN: 2,""%[^\r]\r\n""", rets);
                sCell = rets;
                sCell.replace("\"", "");
                break;
            case 20:
                sscanf(buf, "\r\n %[^\r]\r\n", rets);
                sModel = rets;
                break;
            case 21:
                sscanf(buf, "\r\n %[^\r]\r\n", rets);
                sHw = rets;
                break;
            case 22:
                sscanf(buf, "\r\n %[^\r]\r\n", rets);
                sFw = rets;
                break;
            case 23:
                sscanf(buf, "\r\n+UGPIOR: %[^\r]\r\n", rets);
                token = strtok(rets, s);
                token = strtok(NULL, s);
                sSim = token;
                if (sSim == "1")
                {
                    sSim = "Internal";
                }
                else
                {
                    sSim = "External";
                }
                break;
        }
    }
    return WAIT;
}
//******************
//Set new MNO number
//******************
void SetMNO(int mno)
{
    char s[256];
    
    AT_cmd = -1;
    
    sprintf(s,"AT+CFUN=0\r\n"); //stop modem
    Cellular.command(callbackAT, at_response, 10000, s);
    delay(50);

    sprintf(s,"AT+UMNOPROF=%d\r\n",mno); //write new MNO
    Cellular.command(callbackAT, at_response, 10000, s);
    delay(50);

    sprintf(s,"AT+CFUN=16\r\n"); //reset and save new MNO to NVM
    Cellular.command(callbackAT, at_response, 10000, s);
    delay(50);
}
//*********************
//Set new LTE Band Mask
//*********************
void SetBANDMASK(char* mask)
{
    char s[256];
    
    AT_cmd = -1;
    
    sprintf(s,"AT+CFUN=0\r\n"); //stop modem
    Cellular.command(callbackAT, at_response, 10000, s);

    sprintf(s,"AT+UBANDMASK=0,%s\r\n",mask); //set new bandmask
    Cellular.command(callbackAT, at_response, 10000, s);

    sprintf(s,"AT+CFUN=16\r\n"); //reset and save new BANDMASK to NVM
    Cellular.command(callbackAT, at_response, 10000, s);
}
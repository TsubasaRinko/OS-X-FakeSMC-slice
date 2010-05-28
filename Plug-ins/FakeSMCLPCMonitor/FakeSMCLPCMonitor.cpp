/*
 *
 *  Copyright © 2010 mozodojo. All rights reserved.
 *
 *	This code includes parts of original Open Hardware Monitor code
 *	Copyright © 2010 Michael Möller. All Rights Reserved.
 *
 */

#include <architecture/i386/pio.h>
//#include <unistd.h>

#include "FakeSMCLPCMonitor.h"

UInt8 ReadByte(UInt8 reg)
{
	outb(RegisterPort, reg);
	return inb(ValuePort);
}

UInt16 ReadWord(UInt8 reg)
{
	return ((ReadByte(reg) << 8) | ReadByte(reg + 1));
}

void Select(UInt8 logicalDeviceNumber)
{
	outb(RegisterPort, DEVCIE_SELECT_REGISTER);
	outb(ValuePort, logicalDeviceNumber);
}

void UpdateChipName()
{
	switch (Model) 
	{
        case F71858: Name = "Fintek F71858"; break;
        case F71862: Name = "Fintek F71862"; break;
        case F71869: Name = "Fintek F71869"; break;
        case F71882: Name = "Fintek F71882"; break;
        case F71889ED: Name = "Fintek F71889ED"; break;
        case F71889F: Name = "Fintek F71889F"; break;
        case IT8712F: Name = "ITE IT8712F"; break;
        case IT8716F: Name = "ITE IT8716F"; break;
        case IT8718F: Name = "ITE IT8718F"; break;
        case IT8720F: Name = "ITE IT8720F"; break;
        case IT8726F: Name = "ITE IT8726F"; break;
        case W83627DHG: Name = "Winbond W83627DHG"; break;
        case W83627DHGP: Name = "Winbond W83627DHG-P"; break;
        case W83627EHF: Name = "Winbond W83627EHF"; break;
        case W83627HF: Name = "Winbond W83627HF"; break;
        case W83627THF: Name = "Winbond W83627THF"; break;
        case W83667HG: Name = "Winbond W83667HG"; break;
        case W83667HGB: Name = "Winbond W83667HG-B"; break;
        case W83687THF: Name = "Winbond W83687THF"; break;
		default: Name = "Unknown";
    };
}

// ITE

void IT87Enter()
{
	outb(RegisterPort, 0x87);
	outb(RegisterPort, 0x01);
	outb(RegisterPort, 0x55);
	outb(RegisterPort, 0x55);
}

void IT87Exit()
{
	outb(RegisterPort, CONFIGURATION_CONTROL_REGISTER);
	outb(ValuePort, 0x02);
}

short IT87ReadByte(UInt8 reg, bool* valid)
{
	outb(Address + ITE_ADDRESS_REGISTER_OFFSET, reg);
	UInt8 value = inb(Address + ITE_DATA_REGISTER_OFFSET);
	
	valid = (bool*)(reg == inb(Address + ITE_DATA_REGISTER_OFFSET));
	
	return value;
}

short IT87ReadRPM(UInt8 num)
{
	bool* valid;
	int value = IT87ReadByte(ITE_FAN_TACHOMETER_REG[num], valid);
	
	if(valid)
	{
		value |= IT87ReadByte(ITE_FAN_TACHOMETER_EXT_REG[num], valid) << 8;
		
		if(valid) 
			if (value > 0x3f) 
				value = 1.35e6f / (value * 2);
	}
	
	return value;
}

// WinbondFintek

void WinbondFintekEnter()
{
	outb(RegisterPort, 0x87);
	outb(RegisterPort, 0x87);
}

void WinbondFintekExit()
{
	outb(RegisterPort, 0xAA);      
}

UInt8 WinbondReadByte(UInt8 bank, UInt8 reg) 
{
	outb((UInt16)(Address + WINBOND_ADDRESS_REGISTER_OFFSET), WINBOND_BANK_SELECT_REGISTER);
	outb((UInt16)(Address + WINBOND_DATA_REGISTER_OFFSET), bank);
	outb((UInt16)(Address + WINBOND_ADDRESS_REGISTER_OFFSET), reg);
	return inb((UInt16)(Address + WINBOND_DATA_REGISTER_OFFSET));
}

void WinbondWriteByte(UInt8 bank, UInt8 reg, UInt8 value)
{
	outb((ushort)(Address + WINBOND_ADDRESS_REGISTER_OFFSET), WINBOND_BANK_SELECT_REGISTER);
	outb((ushort)(Address + WINBOND_DATA_REGISTER_OFFSET), bank);
	outb((ushort)(Address + WINBOND_ADDRESS_REGISTER_OFFSET), reg);
	outb((ushort)(Address + WINBOND_DATA_REGISTER_OFFSET), value); 
}

short WinbondReadTemperature(UInt8 index)
{
	UInt32 value = WinbondReadByte(WINBOND_TEMPERATURE_BANK[index], WINBOND_TEMPERATURE_REG[index]) << 1;
	
	if (WINBOND_TEMPERATURE_BANK[index] > 0) 
		value |= WinbondReadByte(WINBOND_TEMPERATURE_BANK[index], (UInt8)(WINBOND_TEMPERATURE_REG[index] + 1)) >> 7;
	
	float temperature = value / 2.0f;
	
	return temperature;
}

UInt64 WinbondSetBit(UInt64 target, UInt32 bit, UInt32 value)
{
	if (((value & 1) == value) && bit >= 0 && bit <= 63)
	{
		UInt64 mask = (((UInt64)1) << bit);
		return value > 0 ? target | mask : target & ~mask;
	}
	
	return value;
}

UInt16 WinbondReadRPM(UInt8 index)
{
	UInt64 bits = 0;
	
	for (int i = 0; i < 5; i++)
		bits = (bits << 8) | WinbondReadByte(0, WINBOND_FAN_BIT_REG[i]);
	
	UInt64 newBits = bits;
	
	int count = WinbondReadByte(WINBOND_FAN_TACHO_BANK[index], WINBOND_FAN_TACHO_REG[index]);
	
	// assemble fan divisor
	int divisorBits = (int)(
							(((bits >> WINBOND_FAN_DIV_BIT2[index]) & 1) << 2) |
							(((bits >> WINBOND_FAN_DIV_BIT1[index]) & 1) << 1) |
							((bits >> WINBOND_FAN_DIV_BIT0[index]) & 1));
	int divisor = 1 << divisorBits;
	
	float value = (count < 0xff) ? 1.35e6f / (count * divisor) : 0;
	
	// update fan divisor
	if (count > 192 && divisorBits < 7) 
		divisorBits++;
	if (count < 96 && divisorBits > 0)
		divisorBits--;
	
	newBits = WinbondSetBit(newBits, WINBOND_FAN_DIV_BIT2[index], 
							(divisorBits >> 2) & 1);
	newBits = WinbondSetBit(newBits, WINBOND_FAN_DIV_BIT1[index], 
							(divisorBits >> 1) & 1);
	newBits = WinbondSetBit(newBits, WINBOND_FAN_DIV_BIT0[index], 
							divisorBits & 1);
	
	// write new fan divisors 
	for (int i = 4; i >= 0; i--) 
	{
		UInt8 oldByte = (UInt8)(bits & 0xFF);
		UInt8 newByte = (UInt8)(newBits & 0xFF);
		bits = bits >> 8;
		newBits = newBits >> 8;
		if (oldByte != newByte) 
			WinbondWriteByte(0, WINBOND_FAN_BIT_REG[i], newByte);        
	}
	
	if(value < -55 || value > 125)
		return 0;
	
	return value;
}

// SMSC

void SMSCEnter()
{
	outb(RegisterPort, 0x55);
}

void SMSCExit()
{
	outb(RegisterPort, 0xAA);
}

static void Update(const char* key, char* data)
{
	switch (Type)
	{
		case IT87x:
		{
			bool* valid;
			 	
			// Heatsink
			if(CompareKeys(key, "Th0H"))
			{
				char value = IT87ReadByte(ITE_TEMPERATURE_BASE_REG + 1, valid);
				
				if(valid)
				{
					data[0] = value;
					data[1] = 0;
				}
			}
			
			// Northbridge
			if(CompareKeys(key, "TN0P"))
			{
				char value = IT87ReadByte(ITE_TEMPERATURE_BASE_REG, valid);
				
				if(valid)
				{
					data[0] = value;
					data[1] = 0;
				}
			}
			
			// CPU Vcore
			if(CompareKeys(key, "VC0C"))
			{
				UInt16 value = IT87ReadByte(ITE_VOLTAGE_BASE_REG + 0, valid);
				
				if (valid)
				{
					value <<= 4;
					
					data[0] = (value >> 4) & 0xff;
					data[1] = ((value << 4) & 0xff) | 0x3;
				}
			}
			
			// FANs
			if(CompareKeys(key, "F0Ac") || CompareKeys(key, "F1Ac") || CompareKeys(key, "F2Ac") || CompareKeys(key, "F3Ac") || CompareKeys(key, "F4Ac"))
			{
				short value = IT87ReadRPM(key[1] - 48);
																	
				data[0] = (value >> 6) & 0xff;
				data[1] = (value << 2) & 0xff;
			}
			
			break;
		}
			
		case Winbond:
		{
			// Heatsink
			if(CompareKeys(key, "Th0H"))
			{
				data[0] = WinbondReadTemperature(0);
				data[1] = 0;
			}
			
			// Northbridge
			if(CompareKeys(key, "TN0P"))
			{
				data[0] = WinbondReadTemperature(2);
				data[1] = 0;
			}
			
			// Fans
			if(CompareKeys(key, "F0Ac") || CompareKeys(key, "F1Ac") || CompareKeys(key, "F2Ac") || CompareKeys(key, "F3Ac") || CompareKeys(key, "F4Ac"))
			{
				UInt16 value = WinbondReadRPM(key[1] - 48);
				
				data[0] = (value >> 6) & 0xff;
				data[1] = (value << 2) & 0xff;
			}
			
			break;
		}
			
		case Fintek:
		{
			break;
		}
	}
}

#define super IOService
OSDefineMetaClassAndStructors(LPCMonitorPlugin, IOService)

bool LPCMonitorPlugin::init(OSDictionary *properties)
{
	DebugLog("Initialising...");
	
    super::init(properties);
	
	return true;
}

IOService* LPCMonitorPlugin::probe(IOService *provider, SInt32 *score)
{
	DebugLog("Probing...");
	
	if (super::probe(provider, score) != this) return 0;
	
	
	for (int i = 0; i < 2; i++)
	{
        RegisterPort = REGISTER_PORT[i];
        ValuePort = VALUE_PORT[i];
	
		Type = UnknownType;
		Model = UnknownModel;
		
		// Winbond/Fintek
		
		WinbondFintekEnter();
		
		UInt8 id = ReadByte(CHIP_ID_REGISTER);
		UInt8 logicalDeviceNumber = 0;
		
		Revision = ReadByte(CHIP_REVISION_REGISTER);
		
		switch (id) 
		{
			case 0x05:
				switch (Revision) 
					case 0x07:
						Model = F71858;
						logicalDeviceNumber = F71858_HARDWARE_MONITOR_LDN;
						break;
					case 0x41:
						Model = F71882;
						logicalDeviceNumber = FINTEK_HARDWARE_MONITOR_LDN;
						break;   
				break;
				
			case 0x06:
				switch (Revision) 
					case 0x01:
						Model = F71862;
						logicalDeviceNumber = FINTEK_HARDWARE_MONITOR_LDN;
						break;     
				break;
				
			case 0x07:
				switch (Revision)
					case 0x23:
						Model = F71889F;
						logicalDeviceNumber = FINTEK_HARDWARE_MONITOR_LDN;
						break;    
				break;
				
			case 0x08:
				switch (Revision)
					case 0x14:
						Model = F71869;
						logicalDeviceNumber = FINTEK_HARDWARE_MONITOR_LDN;
						break;    
				break;
			case 0x09:
				switch (Revision)
					case 0x09:
						Model = F71889ED;
						logicalDeviceNumber = FINTEK_HARDWARE_MONITOR_LDN;
						break;    
				break;
				
			case 0x52:
				switch (Revision)
					case 0x17:
					case 0x3A:
					case 0x41:
						Model = W83627HF;
						logicalDeviceNumber = WINBOND_HARDWARE_MONITOR_LDN;
						break;
				break;
				
			case 0x82:
				switch (Revision)
					case 0x83:
						Model = W83627THF;
						logicalDeviceNumber = WINBOND_HARDWARE_MONITOR_LDN;
						break;
				break;
				
			case 0x85:
				switch (Revision)
					case 0x41:
						Model = W83687THF;
						logicalDeviceNumber = WINBOND_HARDWARE_MONITOR_LDN;
						break;
				break;
			case 0x88:
				switch (Revision & 0xF0)
					case 0x50:
					case 0x60:
						Model = W83627EHF;
						logicalDeviceNumber = WINBOND_HARDWARE_MONITOR_LDN;
						break;
				break;
				
			case 0xA0:
				switch (Revision & 0xF0)
					case 0x20: 
						Model = W83627DHG;
						logicalDeviceNumber = WINBOND_HARDWARE_MONITOR_LDN;  
						break;   
				break;
				
			case 0xA5:
				switch (Revision & 0xF0)
					case 0x10:
						Model = W83667HG;
						logicalDeviceNumber = WINBOND_HARDWARE_MONITOR_LDN;
						break;
				break;
				
			case 0xB0:
				switch (Revision & 0xF0)
					case 0x70:
						Model = W83627DHGP;
						logicalDeviceNumber = WINBOND_HARDWARE_MONITOR_LDN;
						break;        
				break;
				
			case 0xB3:
				switch (Revision & 0xF0)
					case 0x50:
						Model = W83667HGB;
						logicalDeviceNumber = WINBOND_HARDWARE_MONITOR_LDN;
						break;
				break; 
		}
		
		if (Model == UnknownModel)
		{
			if (id != 0 && id != 0xff)
			{
				WinbondFintekExit();
				
				InfoLog("Found Unknown Winbond / Fintek with ID 0x%x", ((id << 8) | Revision));
				
				return 0;
			}
		} 
		else
		{
			UpdateChipName();
			
			Select(logicalDeviceNumber);
			
			Address = ReadWord(BASE_ADDRESS_REGISTER);          
			
			//usleep(1);
			
			UInt16 verify = ReadWord(BASE_ADDRESS_REGISTER);
			UInt16 vendorID = ReadWord(FINTEK_VENDOR_ID_REGISTER);
			
			WinbondFintekExit();
			
			if (Address != verify)
			{            
				InfoLog("Chip ID - %s", Name); 
				InfoLog("Chip revision - 0x%x", Revision);
				WarningLog("Address verification failed");
				
				return 0;
			}
			
			// some Fintek chips have address register offset 0x05 added already
			if ((Address & 0x07) == 0x05)
				Address &= 0xFFF8;
			
			if (Address < 0x100 || (Address & 0xF007) != 0)
			{            
				InfoLog("Chip ID - %s", Name);
				InfoLog("Chip revision - 0x%x", Revision);
				WarningLog("Invalid address 0x%x", Address);
				
				return 0;
			}
			
			switch (Model)
			{
				case W83627DHG:
				case W83627DHGP:
				case W83627EHF:
				case W83627HF:
				case W83627THF:
				case W83667HG:
				case W83667HGB:
				case W83687THF:
					Type = Winbond;
					break;
					
				case F71858:
				case F71862:
				case F71869:
				case F71882:
				case F71889ED:
				case F71889F:
					if (vendorID != FINTEK_VENDOR_ID)
					{
						InfoLog("Chip ID - %s", Name);
						InfoLog("Chip revision - 0x%x", Revision);
						WarningLog("Invalid vendor ID 0x%x", vendorID);
						
						return 0;
					}
					Type = Fintek;
					break;
					
				default: 
					return 0;
			}
			
			UpdateChipName();
			
			InfoLog("Found %s", Name);
			
			break;
		}
		
		// ITE
		
		IT87Enter();
		
		UInt16 chipID = ReadWord(CHIP_ID_REGISTER);
		
		switch (chipID)
		{
			case 0x8712: 
				Model = IT8712F; 
				break;
			case 0x8716: 
				Model = IT8716F; 
				break;
			case 0x8718: 
				Model = IT8718F; 
				break;
			case 0x8720: 
				Model = IT8720F; 
				break;
			case 0x8726: 
				Model = IT8726F; 
				break; 
			default: 
				Model = UnknownModel;
				break;
		}
		
		if (Model == UnknownModel)
		{
			if (chipID != 0 && chipID != 0xffff)
			{
				IT87Exit();
				
				Type = IT87x;
				
				InfoLog("Found unknown ITE with ID 0x%x", chipID);
				
				break;
			}
			else 
			{
				return 0;
			}

		} 
		else 
		{
			UpdateChipName();
			
			Select(IT87_ENVIRONMENT_CONTROLLER_LDN);
			
			Address = ReadWord(BASE_ADDRESS_REGISTER);
			
			//Thread.Sleep(1);
			
			UInt16 verify = ReadWord(BASE_ADDRESS_REGISTER);
			
			IT87Exit();
			
			if (Address != verify || Address < 0x100 || (Address & 0xF007) != 0)
			{
				InfoLog("Chip ID - %s", Name);
				WarningLog("Invalid address 0x%x", Address);
				
				return 0;
			}
			
			Type = IT87x;
			
			InfoLog("Found %s", Name);
			
			break;
		}
		
		// SMSC
		SMSCEnter();
		
		chipID = ReadWord(CHIP_ID_REGISTER);
		
		switch (chipID) 
		{
			default: Model = UnknownModel; break;
		}
		
		if (Model == UnknownModel)
		{
			if (chipID != 0 && chipID != 0xffff)
			{
				SMSCExit();
				
				UpdateChipName();
				
				InfoLog("Found unknown SMSC with ID 0x%x", chipID);
				
				return 0;
			}
		} 
		else 
		{
			SMSCExit();
			
			return 0;
		}
	}
	
	return this;
}

bool LPCMonitorPlugin::start(IOService * provider)
{
	DebugLog("Starting...");
	
	if (!super::start(provider)) return false;
	
	OSArray* fanIDs = OSDynamicCast(OSArray, getProperty("FANs"));
	OSString* fanName[5]; 
	
	if (fanIDs) fanIDs = OSArray::withArray(fanIDs);
	
    if (fanIDs) 
	{
		UInt32 count = fanIDs->getCount();
		
		if(count > 5) count = 5;
		
		for (UInt32 i = 0; i < count; i++)
			fanName[i] = OSDynamicCast(OSString, fanIDs->getObject(i));
		
		fanIDs->release();
    }
	
	// Fans
	switch (Type)
	{
		case IT87x:
		case Winbond:
			char value[2];
			
			for (int i=0; i<5; i++) 
			{
				char key[5];
				
				if (fanName[i] && fanName[i]->getLength() > 0)
				{	
					snprintf(key, 5, "F%dID", i);
					FakeSMCAddKey(key, "ch8*", fanName[i]->getLength(), (char*)fanName[i]->getCStringNoCopy());
				}
				
				value[0] = ((UInt16)100 >> 6) & 0xff;
				value[1] = ((UInt16)100 << 2) & 0xff;
				snprintf(key, 5, "F%dMn", i);
				FakeSMCAddKey(key, "fpe2", 2, value);
				
				value[0] = (6000 >> 6) & 0xff;
				value[1] = (6000 << 2) & 0xff;
				snprintf(key, 5, "F%dMx", i);
				FakeSMCAddKey(key, "fpe2", 2, value);
				
				value[0] = 0x0;
				value[1] = 0x0;
				snprintf(key, 5, "F%dAc", i);
				FakeSMCAddKeyCallback(key, "fp2e", 2, value, &Update);
			}
			
			value[0] = 5;
			FakeSMCAddKey("FNum", 1, value);
			
			break;
	}
	
	// Other sensors
	switch (Type)
	{
		case IT87x:
		{
			bool* valid;
			UInt8 vendorId;
			
			vendorId = IT87ReadByte(ITE_VENDOR_ID_REGISTER, valid);
			
			if (!valid || vendorId != ITE_VENDOR_ID)
			{
				WarningLog("Invalid vendor ID 0x%x, must be 0x%x", vendorId, ITE_VENDOR_ID);
				return false;
			}
			
			if ((IT87ReadByte(ITE_CONFIGURATION_REGISTER, valid) & 0x10) == 0)
			{
				WarningLog("Bit 0x10 of the configuration register should always be 1");
				return false;
			}
			
			if (!valid)
			{
				WarningLog("Invalid %s reading", Name);
				return false;
			}
			
			char value[2];
			
			// Heatsink
			FakeSMCAddKeyCallback("Th0H", "sp78", 2, value, &Update);
			
			// Northbridge
			FakeSMCAddKeyCallback("TN0P", "sp78", 2, value, &Update);
			
			// CPU Vcore
			FakeSMCAddKeyCallback("VC0C", "fp2e", 2, value, &Update);
			
			break;
		}
			
		case Winbond:
		{
			char value[2];
			
			switch (Model) 
			{
				case W83667HG:
				case W83667HGB:
				{
					// do not add temperature sensor registers that read PECI
					UInt8 flag = WinbondReadByte(0, WINBOND_TEMPERATURE_SOURCE_SELECT_REG);
					
					// Heatsink
					if ((flag & 0x04) == 0)	FakeSMCAddKeyCallback("Th0H", "sp78", 2, value, &Update);
					
					/*if ((flag & 0x40) == 0)
						list.Add(new Sensor(TEMPERATURE_NAME[1], 1, null,
											SensorType.Temperature, this, parameter));*/
					
					// Northbridge
					FakeSMCAddKeyCallback("TN0P", "sp78", 2, value, &Update);
					
					break;
				}
					
				case W83627DHG:        
				case W83627DHGP:
				{
					// do not add temperature sensor registers that read PECI
					UInt8 sel = WinbondReadByte(0, WINBOND_TEMPERATURE_SOURCE_SELECT_REG);
					
					// Heatsink
					if ((sel & 0x07) == 0) FakeSMCAddKeyCallback("Th0H", "sp78", 2, value, &Update);
					
					/*if ((sel & 0x70) == 0)
						list.Add(new Sensor(TEMPERATURE_NAME[1], 1, null,
											SensorType.Temperature, this, parameter));*/
					
					// Northbridge
					FakeSMCAddKeyCallback("TN0P", "sp78", 2, value, &Update);
					
					break;
				}
					
				default:
				{
					// no PECI support, add all sensors
					// Heatsink
					FakeSMCAddKeyCallback("Th0H", "sp78", 2, value, &Update);
					// Northbridge
					FakeSMCAddKeyCallback("TN0P", "sp78", 2, value, &Update);
				  break;
				}
			}
			
			break;
		}
			
		case Fintek:
		{
			
			break;
		}
	}
	
	return true;	
}

void LPCMonitorPlugin::stop (IOService* provider)
{
	DebugLog("Stoping...");
	
	switch (Type)
	{
		case IT87x:
			FakeSMCRemoveKeyCallback("VC0C");
		case Winbond:
			FakeSMCRemoveKeyCallback("Th0H");
			FakeSMCRemoveKeyCallback("TN0P");
			for (int i=0; i<5; i++) 
			{
				char key[5];
				snprintf(key, 5, "F%dAc", i);
				FakeSMCRemoveKeyCallback(key);
			}
			break;
		case Fintek:
			break;

	}
	
	super::stop(provider);
}

void LPCMonitorPlugin::free ()
{
	DebugLog("Freeing...");
	
	super::free ();
}
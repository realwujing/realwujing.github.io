/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20181213 (64-bit version)
 * Copyright (c) 2000 - 2018 Intel Corporation
 * 
 * Disassembling to symbolic ASL+ operators
 *
 * Disassembly of dsdt.dat, Sat Aug 19 17:38:37 2023
 *
 * Original Table Header:
 *     Signature        "DSDT"
 *     Length           0x000015B0 (5552)
 *     Revision         0x02
 *     Checksum         0x48
 *     OEM ID           "PHYLTD"
 *     OEM Table ID     "PHYTIUM."
 *     OEM Revision     0x00000001 (1)
 *     Compiler ID      "INTL"
 *     Compiler Version 0x20181213 (538448403)
 */
DefinitionBlock ("", "DSDT", 2, "PHYLTD", "PHYTIUM.", 0x00000001)
{
    OperationRegion (CPUS, SystemMemory, 0xFBE16028, 0x0010)
    Field (CPUS, AnyAcc, Lock, Preserve)
    {
        Offset (0x08), 
        PST0,   8, 
        PST1,   8, 
        PST2,   8, 
        PST3,   8
    }

    Scope (_SB)
    {
        Device (CLU0)
        {
            Name (_HID, "ACPI0010" /* Processor Container Device */)  // _HID: Hardware ID
            Name (_UID, Zero)  // _UID: Unique ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (0x0F)
            }

            Device (CPU0)
            {
                Name (_HID, "ACPI0007" /* Processor Device */)  // _HID: Hardware ID
                Name (_UID, Zero)  // _UID: Unique ID
                Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
                {
                    ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                    Package (0x02)
                    {
                        Package (0x02)
                        {
                            "clock-name", 
                            "c0"
                        }, 

                        Package (0x02)
                        {
                            "clock-domain", 
                            Zero
                        }
                    }
                })
                Method (_STA, 0, NotSerialized)  // _STA: Status
                {
                    Return (0x0F)
                }
            }

            Device (CPU1)
            {
                Name (_HID, "ACPI0007" /* Processor Device */)  // _HID: Hardware ID
                Name (_UID, One)  // _UID: Unique ID
                Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
                {
                    ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                    Package (0x02)
                    {
                        Package (0x02)
                        {
                            "clock-name", 
                            "c0"
                        }, 

                        Package (0x02)
                        {
                            "clock-domain", 
                            Zero
                        }
                    }
                })
                Method (_STA, 0, NotSerialized)  // _STA: Status
                {
                    Return (0x0F)
                }
            }
        }

        Device (CLU1)
        {
            Name (_HID, "ACPI0010" /* Processor Container Device */)  // _HID: Hardware ID
            Name (_UID, One)  // _UID: Unique ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (0x0F)
            }

            Device (CPU2)
            {
                Name (_HID, "ACPI0007" /* Processor Device */)  // _HID: Hardware ID
                Name (_UID, 0x02)  // _UID: Unique ID
                Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
                {
                    ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                    Package (0x02)
                    {
                        Package (0x02)
                        {
                            "clock-name", 
                            "c1"
                        }, 

                        Package (0x02)
                        {
                            "clock-domain", 
                            One
                        }
                    }
                })
                Method (_STA, 0, NotSerialized)  // _STA: Status
                {
                    Return (0x0F)
                }
            }

            Device (CPU3)
            {
                Name (_HID, "ACPI0007" /* Processor Device */)  // _HID: Hardware ID
                Name (_UID, 0x03)  // _UID: Unique ID
                Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
                {
                    ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                    Package (0x02)
                    {
                        Package (0x02)
                        {
                            "clock-name", 
                            "c1"
                        }, 

                        Package (0x02)
                        {
                            "clock-domain", 
                            One
                        }
                    }
                })
                Method (_STA, 0, NotSerialized)  // _STA: Status
                {
                    Return (0x0F)
                }
            }
        }

        Device (CLU2)
        {
            Name (_HID, "ACPI0010" /* Processor Container Device */)  // _HID: Hardware ID
            Name (_UID, 0x02)  // _UID: Unique ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (0x0F)
            }

            Device (CPU4)
            {
                Name (_HID, "ACPI0007" /* Processor Device */)  // _HID: Hardware ID
                Name (_UID, 0x04)  // _UID: Unique ID
                Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
                {
                    ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                    Package (0x02)
                    {
                        Package (0x02)
                        {
                            "clock-name", 
                            "c2"
                        }, 

                        Package (0x02)
                        {
                            "clock-domain", 
                            0x02
                        }
                    }
                })
                Method (_STA, 0, NotSerialized)  // _STA: Status
                {
                    Return (0x0F)
                }
            }

            Device (CPU5)
            {
                Name (_HID, "ACPI0007" /* Processor Device */)  // _HID: Hardware ID
                Name (_UID, 0x05)  // _UID: Unique ID
                Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
                {
                    ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                    Package (0x02)
                    {
                        Package (0x02)
                        {
                            "clock-name", 
                            "c2"
                        }, 

                        Package (0x02)
                        {
                            "clock-domain", 
                            0x02
                        }
                    }
                })
                Method (_STA, 0, NotSerialized)  // _STA: Status
                {
                    Return (0x0F)
                }
            }
        }

        Device (CLU3)
        {
            Name (_HID, "ACPI0010" /* Processor Container Device */)  // _HID: Hardware ID
            Name (_UID, 0x03)  // _UID: Unique ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (0x0F)
            }

            Device (CPU6)
            {
                Name (_HID, "ACPI0007" /* Processor Device */)  // _HID: Hardware ID
                Name (_UID, 0x06)  // _UID: Unique ID
                Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
                {
                    ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                    Package (0x02)
                    {
                        Package (0x02)
                        {
                            "clock-name", 
                            "c3"
                        }, 

                        Package (0x02)
                        {
                            "clock-domain", 
                            0x03
                        }
                    }
                })
                Method (_STA, 0, NotSerialized)  // _STA: Status
                {
                    Return (0x0F)
                }
            }

            Device (CPU7)
            {
                Name (_HID, "ACPI0007" /* Processor Device */)  // _HID: Hardware ID
                Name (_UID, 0x07)  // _UID: Unique ID
                Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
                {
                    ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                    Package (0x02)
                    {
                        Package (0x02)
                        {
                            "clock-name", 
                            "c3"
                        }, 

                        Package (0x02)
                        {
                            "clock-domain", 
                            0x03
                        }
                    }
                })
                Method (_STA, 0, NotSerialized)  // _STA: Status
                {
                    Return (0x0F)
                }
            }
        }
    }

    Scope (_SB)
    {
        Device (UAR1)
        {
            Name (_HID, "ARMH0011")  // _HID: Hardware ID
            Name (_UID, One)  // _UID: Unique ID
            Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
            {
                Memory32Fixed (ReadWrite,
                    0x28001000,         // Address Base
                    0x00001000,         // Address Length
                    )
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
                {
                    0x00000027,
                }
            })
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (0x0F)
            }
        }

        Device (UAR0)
        {
            Name (_HID, "ARMH0011")  // _HID: Hardware ID
            Name (_UID, Zero)  // _UID: Unique ID
            Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
            {
                Memory32Fixed (ReadWrite,
                    0x28000000,         // Address Base
                    0x00001000,         // Address Length
                    )
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
                {
                    0x00000026,
                }
            })
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (0x0F)
            }
        }
    }

    Scope (_SB)
    {
        Device (GPI0)
        {
            Name (_HID, "PHYT0001")  // _HID: Hardware ID
            Name (_CID, "FTGP0001")  // _CID: Compatible ID
            Name (_UID, Zero)  // _UID: Unique ID
            Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
            {
                Memory32Fixed (ReadWrite,
                    0x28004000,         // Address Base
                    0x00001000,         // Address Length
                    )
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
                {
                    0x0000002A,
                }
            })
            Device (GP00)
            {
                Name (_UID, Zero)  // _UID: Unique ID
                Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
                {
                    ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                    Package (0x03)
                    {
                        Package (0x02)
                        {
                            "reg", 
                            Zero
                        }, 

                        Package (0x02)
                        {
                            "snps,nr-gpios", 
                            0x08
                        }, 

                        Package (0x02)
                        {
                            "nr-gpios", 
                            0x08
                        }
                    }
                })
            }

            Device (GP01)
            {
                Name (_UID, One)  // _UID: Unique ID
                Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
                {
                    ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                    Package (0x03)
                    {
                        Package (0x02)
                        {
                            "reg", 
                            Zero
                        }, 

                        Package (0x02)
                        {
                            "snps,nr-gpios", 
                            0x08
                        }, 

                        Package (0x02)
                        {
                            "nr-gpios", 
                            0x08
                        }
                    }
                })
            }
        }
    }

    Scope (_SB)
    {
        Device (CLK2)
        {
            Name (_HID, "PHYT8002")  // _HID: Hardware ID
            Name (_CID, "FTCK0002")  // _CID: Compatible ID
            Name (_UID, One)  // _UID: Unique ID
            Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                Package (0x01)
                {
                    Package (0x02)
                    {
                        "clock-frequency", 
                        0x02DC6C00
                    }
                }
            })
            Method (FREQ, 0, NotSerialized)
            {
                Return (0x02DC6C00)
            }
        }

        Device (I2C0)
        {
            Name (_HID, "PHYT0003")  // _HID: Hardware ID
            Name (_CID, "FTI20001")  // _CID: Compatible ID
            Name (_UID, Zero)  // _UID: Unique ID
            Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
            {
                Memory32Fixed (ReadWrite,
                    0x28006000,         // Address Base
                    0x00001000,         // Address Length
                    )
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
                {
                    0x0000002C,
                }
            })
            Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                Package (0x02)
                {
                    Package (0x02)
                    {
                        "clock-frequency", 
                        0x000186A0
                    }, 

                    Package (0x02)
                    {
                        "clocks", 
                        Package (0x01)
                        {
                            "\\_SB.CLK2"
                        }
                    }
                }
            })
            Device (EPR0)
            {
                Name (_HID, "INT0002" /* Virtual GPIO Controller */)  // _HID: Hardware ID
                Name (_UID, Zero)  // _UID: Unique ID
                Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
                {
                    ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                    Package (0x01)
                    {
                        Package (0x02)
                        {
                            "pagesize", 
                            0x10
                        }
                    }
                })
                Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
                {
                    I2cSerialBusV2 (0x0057, ControllerInitiated, 0x00061A80,
                        AddressingMode7Bit, "\\_SB.I2C0",
                        0x00, ResourceConsumer, , Exclusive,
                        )
                })
            }
        }

        Device (I2C2)
        {
            Name (_HID, "PHYT0003")  // _HID: Hardware ID
            Name (_CID, "FTI20001")  // _CID: Compatible ID
            Name (_UID, 0x02)  // _UID: Unique ID
            Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
            {
                Memory32Fixed (ReadWrite,
                    0x28008000,         // Address Base
                    0x00001000,         // Address Length
                    )
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
                {
                    0x0000002E,
                }
            })
            Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                Package (0x02)
                {
                    Package (0x02)
                    {
                        "clock-frequency", 
                        0x000186A0
                    }, 

                    Package (0x02)
                    {
                        "clocks", 
                        Package (0x01)
                        {
                            "\\_SB.CLK2"
                        }
                    }
                }
            })
        }
    }

    Scope (_SB)
    {
        Device (SDC0)
        {
            Name (_HID, "PHYT0005")  // _HID: Hardware ID
            Name (_CID, "FTSD0001")  // _CID: Compatible ID
            Name (_UID, Zero)  // _UID: Unique ID
            Name (_CCA, One)  // _CCA: Cache Coherency Attribute
            Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
            {
                Memory32Fixed (ReadWrite,
                    0x28207C00,         // Address Base
                    0x00000100,         // Address Length
                    )
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
                {
                    0x00000034,
                    0x00000035,
                    0x00000036,
                }
            })
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (0x0F)
            }
        }
    }

    Scope (_SB)
    {
        Device (HDA0)
        {
            Name (_HID, "PHYT0006")  // _HID: Hardware ID
            Name (_CID, "FTHD0001")  // _CID: Compatible ID
            Name (_UID, Zero)  // _UID: Unique ID
            Name (_CCA, One)  // _CCA: Cache Coherency Attribute
            Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
            {
                Memory32Fixed (ReadWrite,
                    0x28206000,         // Address Base
                    0x00001000,         // Address Length
                    )
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
                {
                    0x00000037,
                }
            })
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (0x0F)
            }
        }
    }

    Scope (_SB)
    {
        Device (CLK1)
        {
            Name (_HID, "PHYT8001")  // _HID: Hardware ID
            Name (_CID, "FTCK0001")  // _CID: Compatible ID
            Name (_UID, Zero)  // _UID: Unique ID
            Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                Package (0x01)
                {
                    Package (0x02)
                    {
                        "compatible", 
                        "arm,scpi-clocks"
                    }
                }
            })
            Device (CK10)
            {
                Name (_ADR, Zero)  // _ADR: Address
                Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
                {
                    ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                    Package (0x04)
                    {
                        Package (0x02)
                        {
                            "compatible", 
                            "arm,scpi-dvfs-clocks"
                        }, 

                        Package (0x02)
                        {
                            "#clock-cells", 
                            One
                        }, 

                        Package (0x02)
                        {
                            "clock-indices", 
                            Package (0x04)
                            {
                                Zero, 
                                One, 
                                0x02, 
                                0x03
                            }
                        }, 

                        Package (0x02)
                        {
                            "clock-output-names", 
                            Package (0x04)
                            {
                                "c0", 
                                "c1", 
                                "c2", 
                                "c3"
                            }
                        }
                    }
                })
            }
        }

        Device (SCPI)
        {
            Name (_HID, "PHYT0008")  // _HID: Hardware ID
            Name (_CID, "FTSC0001")  // _CID: Compatible ID
            Name (_UID, Zero)  // _UID: Unique ID
            Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                Package (0x03)
                {
                    Package (0x02)
                    {
                        "mbox", 
                        MBX0
                    }, 

                    Package (0x02)
                    {
                        "shmem_start", 
                        0x2A007000
                    }, 

                    Package (0x02)
                    {
                        "shmem_size", 
                        0x0800
                    }
                }
            })
            Device (SEN1)
            {
                Name (_HID, "PHYT000D")  // _HID: Hardware ID
                Name (_CID, "FTSS0001")  // _CID: Compatible ID
                Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
                {
                    ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                    Package (0x02)
                    {
                        Package (0x02)
                        {
                            "compatible", 
                            "arm,scpi-sensors"
                        }, 

                        Package (0x02)
                        {
                            "#thermal-sensor-cells", 
                            One
                        }
                    }
                })
            }
        }

        Device (MBX0)
        {
            Name (_HID, "PHYT0009")  // _HID: Hardware ID
            Name (_CID, "FTMB0001")  // _CID: Compatible ID
            Name (_UID, Zero)  // _UID: Unique ID
            Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
            {
                Memory32Fixed (ReadWrite,
                    0x2A000000,         // Address Base
                    0x00001000,         // Address Length
                    )
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
                {
                    0x00000050,
                }
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
                {
                    0x0000004F,
                }
            })
        }
    }

    Scope (_SB)
    {
        Device (CLK3)
        {
            Name (_HID, "PHYT8002")  // _HID: Hardware ID
            Name (_CID, "FTCK0002")  // _CID: Compatible ID
            Name (_UID, Zero)  // _UID: Unique ID
            Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                Package (0x01)
                {
                    Package (0x02)
                    {
                        "clock-frequency", 
                        0x23C34600
                    }
                }
            })
            Method (FREQ, 0, NotSerialized)
            {
                Return (0x23C34600)
            }
        }

        Device (CAN0)
        {
            Name (_HID, "PHYT000A")  // _HID: Hardware ID
            Name (_CID, "FTCN0001")  // _CID: Compatible ID
            Name (_UID, Zero)  // _UID: Unique ID
            Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
            {
                Memory32Fixed (ReadWrite,
                    0x28207000,         // Address Base
                    0x00000400,         // Address Length
                    )
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
                {
                    0x00000077,
                }
            })
            Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                Package (0x05)
                {
                    Package (0x02)
                    {
                        "clocks", 
                        Package (0x01)
                        {
                            "\\_SB.CLK3"
                        }
                    }, 

                    Package (0x02)
                    {
                        "clock-frequency", 
                        0x23C34600
                    }, 

                    Package (0x02)
                    {
                        "clock-names", 
                        "ftcan_clk"
                    }, 

                    Package (0x02)
                    {
                        "tx-fifo-depth", 
                        0x40
                    }, 

                    Package (0x02)
                    {
                        "rx-fifo-depth", 
                        0x40
                    }
                }
            })
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (0x0F)
            }
        }

        Device (CAN1)
        {
            Name (_HID, "PHYT000A")  // _HID: Hardware ID
            Name (_CID, "FTCN0001")  // _CID: Compatible ID
            Name (_UID, One)  // _UID: Unique ID
            Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
            {
                Memory32Fixed (ReadWrite,
                    0x28207400,         // Address Base
                    0x00000400,         // Address Length
                    )
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
                {
                    0x0000007B,
                }
            })
            Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                Package (0x05)
                {
                    Package (0x02)
                    {
                        "clocks", 
                        Package (0x01)
                        {
                            "\\_SB.CLK3"
                        }
                    }, 

                    Package (0x02)
                    {
                        "clock-frequency", 
                        0x23C34600
                    }, 

                    Package (0x02)
                    {
                        "clock-names", 
                        "ftcan_clk"
                    }, 

                    Package (0x02)
                    {
                        "tx-fifo-depth", 
                        0x40
                    }, 

                    Package (0x02)
                    {
                        "rx-fifo-depth", 
                        0x40
                    }
                }
            })
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (0x0F)
            }
        }

        Device (CAN2)
        {
            Name (_HID, "PHYT000A")  // _HID: Hardware ID
            Name (_CID, "FTCN0001")  // _CID: Compatible ID
            Name (_UID, 0x02)  // _UID: Unique ID
            Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
            {
                Memory32Fixed (ReadWrite,
                    0x28207800,         // Address Base
                    0x00000400,         // Address Length
                    )
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
                {
                    0x0000007C,
                }
            })
            Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                Package (0x05)
                {
                    Package (0x02)
                    {
                        "clocks", 
                        Package (0x01)
                        {
                            "\\_SB.CLK3"
                        }
                    }, 

                    Package (0x02)
                    {
                        "clock-frequency", 
                        0x23C34600
                    }, 

                    Package (0x02)
                    {
                        "clock-names", 
                        "ftcan_clk"
                    }, 

                    Package (0x02)
                    {
                        "tx-fifo-depth", 
                        0x40
                    }, 

                    Package (0x02)
                    {
                        "rx-fifo-depth", 
                        0x40
                    }
                }
            })
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (0x0F)
            }
        }
    }

    Scope (_SB)
    {
        Device (SPI1)
        {
            Name (_HID, "PHTY000E")  // _HID: Hardware ID
            Name (_UID, Zero)  // _UID: Unique ID
            Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
            {
                GpioIo (Exclusive, PullUp, 0x0000, 0x0000, IoRestrictionNone,
                    "\\_SB.GPI0", 0x00, ResourceConsumer, ,
                    )
                    {   // Pin list
                        0x000D
                    }
                Memory32Fixed (ReadWrite,
                    0x28013000,         // Address Base
                    0x00001000,         // Address Length
                    )
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
                {
                    0x00000033,
                }
            })
            Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                Package (0x01)
                {
                    Package (0x02)
                    {
                        "cs-gpios", 
                        Package (0x04)
                        {
                            SPI1, 
                            Zero, 
                            Zero, 
                            Zero
                        }
                    }
                }
            })
            Device (TPM)
            {
                Name (_ADR, Zero)  // _ADR: Address
                Name (_CID, Package (0x01)  // _CID: Compatible ID
                {
                    "SMO0768"
                })
                Name (_UID, Zero)  // _UID: Unique ID
                Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
                {
                    ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                    Package (0x00){}
                })
                Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
                {
                    Name (RBUF, ResourceTemplate ()
                    {
                        SpiSerialBusV2 (0x0000, PolarityLow, FourWireMode, 0x08,
                            ControllerInitiated, 0x000F4240, ClockPolarityLow,
                            ClockPhaseFirst, "\\_SB.SPI1",
                            0x00, ResourceConsumer, , Exclusive,
                            )
                    })
                    Return (RBUF) /* \_SB_.SPI1.TPM_._CRS.RBUF */
                }
            }
        }
    }

    Scope (_SB)
    {
        Device (LPC1)
        {
            Name (_HID, "PHYT0007")  // _HID: Hardware ID
            Name (_CID, "LPC0001")  // _CID: Compatible ID
            Name (_UID, Zero)  // _UID: Unique ID
            Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
            {
                Memory32Fixed (ReadWrite,
                    0x20000000,         // Address Base
                    0x07FFFFFD,         // Address Length
                    )
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
                {
                    0x00000025,
                }
            })
        }

        Device (PWRB)
        {
            Name (_HID, "PNP0C0C" /* Power Button Device */)  // _HID: Hardware ID
            Name (_UID, Zero)  // _UID: Unique ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (0x0F)
            }
        }

        Device (EC0)
        {
            Name (_HID, "PNP0C09" /* Embedded Controller Device */)  // _HID: Hardware ID
            Name (_UID, Zero)  // _UID: Unique ID
            Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
            {
                IO (Decode16,
                    0x0062,             // Range Minimum
                    0x0062,             // Range Maximum
                    0x00,               // Alignment
                    0x01,               // Length
                    )
                IO (Decode16,
                    0x0066,             // Range Minimum
                    0x0066,             // Range Maximum
                    0x00,               // Alignment
                    0x01,               // Length
                    )
                GpioInt (Edge, ActiveLow, ExclusiveAndWake, PullUp, 0x0000,
                    "\\_SB.GPI0", 0x00, ResourceConsumer, ,
                    )
                    {   // Pin list
                        0x0007
                    }
            })
            Name (H8DR, Zero)
            Method (_REG, 2, NotSerialized)  // _REG: Region Availability
            {
                If ((Arg0 == 0x03))
                {
                    H8DR = Arg1
                }
            }

            OperationRegion (UPD, SystemMemory, 0x58100000, 0x1000)
            Field (UPD, ByteAcc, NoLock, Preserve)
            {
                Offset (0x420), 
                UPS1,   16, 
                Offset (0x440), 
                UPS3,   16, 
                Offset (0x450), 
                UPS4,   16
            }

            OperationRegion (ERAM, EmbeddedControl, Zero, 0x0100)
            Field (ERAM, ByteAcc, NoLock, WriteAsZeros)
            {
                Offset (0x0E), 
                BKLL,   8, 
                Offset (0x21), 
                RCPT,   8, 
                Offset (0x24), 
                FCCL,   8, 
                FCCH,   8, 
                RMCL,   8, 
                RMCH,   8, 
                TMPL,   8, 
                TMPH,   8, 
                VOLL,   8, 
                VOLH,   8, 
                CURL,   8, 
                CURH,   8, 
                AVRL,   8, 
                AVRH,   8, 
                Offset (0x38), 
                DGCL,   8, 
                DGCH,   8, 
                DGVL,   8, 
                DGVH,   8, 
                BSTS,   8, 
                Offset (0x3E), 
                BSNL,   8, 
                BSNH,   8, 
                Offset (0x4E), 
                LPOL,   1, 
                Offset (0x5A), 
                TCPU,   8, 
                Offset (0x97), 
                FANV,   1, 
                Offset (0x98), 
                Offset (0xB0), 
                PPRS,   8, 
                Offset (0xE6), 
                CAP,    8
            }

            Method (_Q21, 0, NotSerialized)  // _Qxx: EC Query, xx=0x00-0xFF
            {
                Notify (BAT0, 0x81) // Information Change
            }

            Method (_Q22, 0, NotSerialized)  // _Qxx: EC Query, xx=0x00-0xFF
            {
                Sleep (0x012C)
                If ((UPS1 == 0x0340))
                {
                    CAP = 0x40
                    Local0 = (UPS1 | 0x10)
                    UPS1 = Local0
                }

                Sleep (0x012C)
                If ((UPS3 == 0x0340))
                {
                    CAP = 0x40
                    Local1 = (UPS3 | 0x10)
                    UPS3 = Local1
                }

                Sleep (0x012C)
                If ((UPS4 == 0x0340))
                {
                    CAP = 0x40
                    Local2 = (UPS4 | 0x10)
                    UPS4 = Local2
                }
            }

            Method (_QB3, 0, NotSerialized)  // _Qxx: EC Query, xx=0x00-0xFF
            {
                Notify (BAT0, 0x80) // Status Change
            }

            Method (_QB4, 0, NotSerialized)  // _Qxx: EC Query, xx=0x00-0xFF
            {
                Notify (AC0, 0x80) // Status Change
            }

            Method (_QB5, 0, NotSerialized)  // _Qxx: EC Query, xx=0x00-0xFF
            {
                Notify (PWRB, 0x80) // Status Change
            }

            Method (_QD0, 0, NotSerialized)  // _Qxx: EC Query, xx=0x00-0xFF
            {
                Notify (LID0, 0x80) // Status Change
            }
        }

        Device (BAT0)
        {
            Name (_HID, "PNP0C0A" /* Control Method Battery */)  // _HID: Hardware ID
            Name (_UID, Zero)  // _UID: Unique ID
            Name (B1ST, 0x1F)
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                If (^^EC0.H8DR)
                {
                    Local1 = ^^EC0.PPRS /* \_SB_.EC0_.PPRS */
                    Local2 = (Local1 & 0x02)
                    If (Local2)
                    {
                        B1ST = 0x1F
                    }
                    Else
                    {
                        B1ST = 0x0F
                    }
                }
                Else
                {
                    B1ST = 0x0F
                }

                Return (B1ST) /* \_SB_.BAT0.B1ST */
            }

            Name (PBIX, Package (0x15)
            {
                One, 
                One, 
                0xFFFFFFFF, 
                0xFFFFFFFF, 
                One, 
                0x3C28, 
                0xFA, 
                0x64, 
                0xFFFFFFFF, 
                0xC350, 
                0xFFFFFFFF, 
                0xFFFFFFFF, 
                0x03E8, 
                0x01F4, 
                0xFFFFFFFF, 
                0xFFFFFFFF, 
                "SNBL001", 
                "1209-00625", 
                "LI-ION", 
                "SCUD", 
                Zero
            })
            Name (PBST, Package (0x04)
            {
                Zero, 
                0xFFFFFFFF, 
                0xFFFFFFFF, 
                0xFFFFFFFF
            })
            Method (_BIX, 0, NotSerialized)  // _BIX: Battery Information Extended
            {
                UBIX ()
                Return (PBIX) /* \_SB_.BAT0.PBIX */
            }

            Method (_BST, 0, NotSerialized)  // _BST: Battery Status
            {
                UBST ()
                Return (PBST) /* \_SB_.BAT0.PBST */
            }

            Method (UBIX, 0, NotSerialized)
            {
                If (^^EC0.H8DR)
                {
                    Local1 = ^^EC0.DGCL /* \_SB_.EC0_.DGCL */
                    Local2 = ^^EC0.DGCH /* \_SB_.EC0_.DGCH */
                    Local2 <<= 0x08
                    Local2 |= Local1
                    PBIX [0x02] = Local2
                    Local1 = ^^EC0.FCCL /* \_SB_.EC0_.FCCL */
                    Local2 = ^^EC0.FCCH /* \_SB_.EC0_.FCCH */
                    Local2 <<= 0x08
                    Local2 |= Local1
                    PBIX [0x03] = Local2
                }
            }

            Method (UBST, 0, NotSerialized)
            {
                If (^^EC0.H8DR)
                {
                    Local1 = ^^EC0.PPRS /* \_SB_.EC0_.PPRS */
                    Local2 = ^^EC0.BSTS /* \_SB_.EC0_.BSTS */
                    Local3 = (Local1 & One)
                    Local4 = (Local1 & 0x02)
                    Local5 = (Local2 & 0x20)
                    If (Local4)
                    {
                        If (Local3)
                        {
                            If (Local5)
                            {
                                PBST [Zero] = Zero
                            }
                            Else
                            {
                                PBST [Zero] = 0x02
                            }
                        }
                        Else
                        {
                            PBST [Zero] = One
                        }
                    }
                    Else
                    {
                        PBST [Zero] = ^^EC0.BSTS /* \_SB_.EC0_.BSTS */
                    }

                    Local1 = ^^EC0.CURL /* \_SB_.EC0_.CURL */
                    Local2 = ^^EC0.CURH /* \_SB_.EC0_.CURH */
                    Local2 <<= 0x08
                    Local2 |= Local1
                    PBST [One] = Local2
                    Local1 = ^^EC0.RMCL /* \_SB_.EC0_.RMCL */
                    Local2 = ^^EC0.RMCH /* \_SB_.EC0_.RMCH */
                    Local2 <<= 0x08
                    Local2 |= Local1
                    PBST [0x02] = Local2
                    Local1 = ^^EC0.VOLL /* \_SB_.EC0_.VOLL */
                    Local2 = ^^EC0.VOLH /* \_SB_.EC0_.VOLH */
                    Local2 <<= 0x08
                    Local2 |= Local1
                    PBST [0x03] = Local2
                }
            }
        }

        Device (AC0)
        {
            Name (_HID, "ACPI0003" /* Power Source Device */)  // _HID: Hardware ID
            Name (_UID, Zero)  // _UID: Unique ID
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (0x0F)
            }

            Method (_PSR, 0, NotSerialized)  // _PSR: Power Source
            {
                If (^^EC0.H8DR)
                {
                    Local1 = ^^EC0.PPRS /* \_SB_.EC0_.PPRS */
                    Local1 &= One
                    If (Local1)
                    {
                        Return (One)
                    }
                    Else
                    {
                        Return (Zero)
                    }
                }
            }
        }

        Device (LID0)
        {
            Name (_HID, EisaId ("PNP0C0D") /* Lid Device */)  // _HID: Hardware ID
            Name (_UID, Zero)  // _UID: Unique ID
            Method (_LID, 0, NotSerialized)  // _LID: Lid Status
            {
                If (^^EC0.H8DR)
                {
                    Local0 = (^^EC0.LPOL & One)
                    Return (!Local0)
                }
                Else
                {
                    Return (One)
                }
            }
        }

        Device (KBC)
        {
            Name (_HID, "KBCI8042")  // _HID: Hardware ID
            Name (_UID, Zero)  // _UID: Unique ID
            Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                Package (0x02)
                {
                    Package (0x02)
                    {
                        "i8042_command_reg", 
                        0x64
                    }, 

                    Package (0x02)
                    {
                        "i8042_data_reg", 
                        0x60
                    }
                }
            })
        }
    }
}


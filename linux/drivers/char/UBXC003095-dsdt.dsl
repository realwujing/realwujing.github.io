/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20181213 (64-bit version)
 * Copyright (c) 2000 - 2018 Intel Corporation
 * 
 * Disassembling to symbolic ASL+ operators
 *
 * Disassembly of dsdt.dat, Fri Sep  1 11:40:22 2023
 *
 * Original Table Header:
 *     Signature        "DSDT"
 *     Length           0x000015A1 (5537)
 *     Revision         0x02
 *     Checksum         0x7F
 *     OEM ID           "PHYLTD"
 *     OEM Table ID     "PHYTIUM."
 *     OEM Revision     0x20180509 (538445065)
 *     Compiler ID      "INTL"
 *     Compiler Version 0x20190509 (538510601)
 */
DefinitionBlock ("", "DSDT", 2, "PHYLTD", "PHYTIUM.", 0x20180509)
{
    External (GMAC, IntObj)

    OperationRegion (CPUS, SystemMemory, 0xF69C0000, 0x0010)
    Field (CPUS, AnyAcc, Lock, Preserve)
    {
        Offset (0x08), 
        PST0,   8, 
        PST1,   8, 
        PST2,   8, 
        PST3,   8, 
        GMAC,   8
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

        Device (UAR2)
        {
            Name (_HID, "ARMH0011")  // _HID: Hardware ID
            Name (_UID, 0x02)  // _UID: Unique ID
            Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
            {
                Memory32Fixed (ReadWrite,
                    0x28002000,         // Address Base
                    0x00001000,         // Address Length
                    )
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
                {
                    0x00000028,
                }
            })
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (0x0F)
            }
        }

        Device (UAR3)
        {
            Name (_HID, "ARMH0011")  // _HID: Hardware ID
            Name (_UID, 0x03)  // _UID: Unique ID
            Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
            {
                Memory32Fixed (ReadWrite,
                    0x28003000,         // Address Base
                    0x00001000,         // Address Length
                    )
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
                {
                    0x00000029,
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
        Device (ETH0)
        {
            Name (_HID, "PHYT0004")  // _HID: Hardware ID
            Name (_CID, "FTGM0001")  // _CID: Compatible ID
            Name (_UID, Zero)  // _UID: Unique ID
            Name (_CCA, One)  // _CCA: Cache Coherency Attribute
            Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
            {
                Memory32Fixed (ReadWrite,
                    0x2820C000,         // Address Base
                    0x00002000,         // Address Length
                    )
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
                {
                    0x00000051,
                }
            })
            Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                Package (0x0F)
                {
                    Package (0x02)
                    {
                        "interrupt-names", 
                        "macirq"
                    }, 

                    Package (0x02)
                    {
                        "clocks", 
                        0x03
                    }, 

                    Package (0x02)
                    {
                        "clock-names", 
                        "stmmaceth"
                    }, 

                    Package (0x02)
                    {
                        "snps,pbl", 
                        0x10
                    }, 

                    Package (0x02)
                    {
                        "snps,abl", 
                        0x20
                    }, 

                    Package (0x02)
                    {
                        "snps,burst_len", 
                        0x0E
                    }, 

                    Package (0x02)
                    {
                        "snps,multicast-filter-bins", 
                        0x40
                    }, 

                    Package (0x02)
                    {
                        "snps,perfect-filter-entries", 
                        0x41
                    }, 

                    Package (0x02)
                    {
                        "max-frame-size", 
                        0x2328
                    }, 

                    Package (0x02)
                    {
                        "phy-mode", 
                        "rgmii-rxid"
                    }, 

                    Package (0x02)
                    {
                        "clock-frequency", 
                        0x0EE6B280
                    }, 

                    Package (0x02)
                    {
                        "bus_id", 
                        Zero
                    }, 

                    Package (0x02)
                    {
                        "txc-skew-ps", 
                        0x03E8
                    }, 

                    Package (0x02)
                    {
                        "rxc-skew-ps", 
                        0x03E8
                    }, 

                    Package (0x02)
                    {
                        "mdc_clock_selection", 
                        0x05
                    }
                }
            })
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                If ((GMAC != Zero))
                {
                    Return (0x0F)
                }

                Return (Zero)
            }
        }

        Device (ETH1)
        {
            Name (_HID, "PHYT0004")  // _HID: Hardware ID
            Name (_CID, "FTGM0001")  // _CID: Compatible ID
            Name (_UID, One)  // _UID: Unique ID
            Name (_CCA, One)  // _CCA: Cache Coherency Attribute
            Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
            {
                Memory32Fixed (ReadWrite,
                    0x28210000,         // Address Base
                    0x00002000,         // Address Length
                    )
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
                {
                    0x00000052,
                }
            })
            Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                Package (0x0F)
                {
                    Package (0x02)
                    {
                        "interrupt-names", 
                        "macirq"
                    }, 

                    Package (0x02)
                    {
                        "clocks", 
                        0x03
                    }, 

                    Package (0x02)
                    {
                        "clock-names", 
                        "stmmaceth"
                    }, 

                    Package (0x02)
                    {
                        "snps,pbl", 
                        0x10
                    }, 

                    Package (0x02)
                    {
                        "snps,abl", 
                        0x20
                    }, 

                    Package (0x02)
                    {
                        "snps,burst_len", 
                        0x0E
                    }, 

                    Package (0x02)
                    {
                        "snps,multicast-filter-bins", 
                        0x40
                    }, 

                    Package (0x02)
                    {
                        "snps,perfect-filter-entries", 
                        0x41
                    }, 

                    Package (0x02)
                    {
                        "max-frame-size", 
                        0x2328
                    }, 

                    Package (0x02)
                    {
                        "phy-mode", 
                        "rgmii-rxid"
                    }, 

                    Package (0x02)
                    {
                        "clock-frequency", 
                        0x0EE6B280
                    }, 

                    Package (0x02)
                    {
                        "bus_id", 
                        One
                    }, 

                    Package (0x02)
                    {
                        "txc-skew-ps", 
                        0x03E8
                    }, 

                    Package (0x02)
                    {
                        "rxc-skew-ps", 
                        0x03E8
                    }, 

                    Package (0x02)
                    {
                        "mdc_clock_selection", 
                        0x05
                    }
                }
            })
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                If ((GMAC != Zero))
                {
                    Return (0x0F)
                }

                Return (Zero)
            }
        }
    }

    Scope (_SB)
    {
        Device (PWRB)
        {
            Name (_HID, EisaId ("PNP0C0C") /* Power Button Device */)  // _HID: Hardware ID
            Name (_PRW, Package (0x02)  // _PRW: Power Resources for Wake
            {
                0x07, 
                0x04
            })
        }

        Device (GPI0)
        {
            Name (_HID, "FTGP0001")  // _HID: Hardware ID
            Name (_ADR, Zero)  // _ADR: Address
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
                Name (_HID, "FTGP0001")  // _HID: Hardware ID
                Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
                {
                    ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                    Package (0x02)
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
                        }
                    }
                })
            }

            Name (_AEI, ResourceTemplate ()  // _AEI: ACPI Event Interrupts
            {
                GpioInt (Edge, ActiveLow, ExclusiveAndWake, PullUp, 0x0000,
                    "\\_SB.GPI0", 0x00, ResourceConsumer, ,
                    )
                    {   // Pin list
                        0x0007
                    }
            })
            Method (_E07, 0, NotSerialized)  // _Exx: Edge-Triggered GPE, xx=0x00-0xFF
            {
                Notify (PWRB, 0x80) // Status Change
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

        Device (I2C1)
        {
            Name (_HID, "PHYT0003")  // _HID: Hardware ID
            Name (_CID, "FTI20001")  // _CID: Compatible ID
            Name (_UID, One)  // _UID: Unique ID
            Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
            {
                Memory32Fixed (ReadWrite,
                    0x28007000,         // Address Base
                    0x00001000,         // Address Length
                    )
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
                {
                    0x0000002D,
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

        Device (I2C3)
        {
            Name (_HID, "PHYT0003")  // _HID: Hardware ID
            Name (_CID, "FTI20001")  // _CID: Compatible ID
            Name (_UID, 0x03)  // _UID: Unique ID
            Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
            {
                Memory32Fixed (ReadWrite,
                    0x28009000,         // Address Base
                    0x00001000,         // Address Length
                    )
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
                {
                    0x0000002F,
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
        Device (OPTE)
        {
            Name (_HID, "PHYT8003")  // _HID: Hardware ID
            Name (_CID, "FTOP0001")  // _CID: Compatible ID
            Name (_UID, Zero)  // _UID: Unique ID
            Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                Package (0x02)
                {
                    Package (0x02)
                    {
                        "compatible", 
                        "linaro,optee-tz"
                    }, 

                    Package (0x02)
                    {
                        "method", 
                        "smc"
                    }
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
        Device (SPI0)
        {
            Name (_HID, "PHYT000E")  // _HID: Hardware ID
            Name (_UID, Zero)  // _UID: Unique ID
            Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
            {
                GpioIo (Exclusive, PullUp, 0x0000, 0x0000, IoRestrictionNone,
                    "\\_SB.GPI1.GP00", 0x00, ResourceConsumer, ,
                    )
                    {   // Pin list
                        0x0005
                    }
                Memory32Fixed (ReadWrite,
                    0x2800C000,         // Address Base
                    0x00001000,         // Address Length
                    )
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
                {
                    0x00000032,
                }
            })
            Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                Package (0x04)
                {
                    Package (0x02)
                    {
                        "clocks", 
                        Package (0x01)
                        {
                            "\\_SB.CLK2"
                        }
                    }, 

                    Package (0x02)
                    {
                        "clocks-spi", 
                        0x23C34600
                    }, 

                    Package (0x02)
                    {
                        "num-cs", 
                        0x04
                    }, 

                    Package (0x02)
                    {
                        "cs-gpios", 
                        Package (0x04)
                        {
                            SPI0, 
                            Zero, 
                            Zero, 
                            Zero
                        }
                    }
                }
            })
            Device (FLAS)
            {
                Name (_HID, "SPT0001")  // _HID: Hardware ID
                Name (_UID, Zero)  // _UID: Unique ID
                Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
                {
                    ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                    Package (0x02)
                    {
                        Package (0x02)
                        {
                            "reg", 
                            Zero
                        }, 

                        Package (0x02)
                        {
                            "spi-max-frequency", 
                            0x02DC6C00
                        }
                    }
                })
                Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
                {
                    SpiSerialBusV2 (0x0000, PolarityLow, FourWireMode, 0x08,
                        ControllerInitiated, 0x003D0900, ClockPolarityLow,
                        ClockPhaseFirst, "\\_SB.SPI0",
                        0x00, ResourceConsumer, , Exclusive,
                        )
                })
            }
        }
    }
}


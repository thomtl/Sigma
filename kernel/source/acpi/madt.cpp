#include <Sigma/acpi/madt.h>


acpi::madt::madt(): legacy_pic(false) {
    this->table = reinterpret_cast<acpi::madt_header*>(acpi::get_table(acpi::madt_signature));
    this->cpus = types::linked_list<smp::cpu_entry>();
    this->ioapics = types::linked_list<std::pair<uint64_t, uint32_t>>();
    this->isos = types::linked_list<x86_64::apic::interrupt_override>();
}

uint64_t acpi::madt::get_lapic_address(){
    //TODO: Support LAPIC Address Override ID = 5
    return this->table->lapic_addr;
}

void acpi::madt::parse_lapic(uint8_t* item){
    auto* lapic = reinterpret_cast<acpi::madt_lapic*>(item);

    debug_printf("[MADT]: Detected CPU: ACPI UID: %x, APIC id: %x", lapic->acpi_uid, lapic->apic_id);
    uint32_t flags = lapic->flags;
    if(bitops<uint32_t>::bit_test(flags, acpi::madt_lapic_flags_enabled) || (!bitops<uint32_t>::bit_test(flags, acpi::madt_lapic_flags_enabled) && bitops<uint32_t>::bit_test(flags, acpi::madt_lapic_flags_online_capable))){
        bool bsp;
        uint32_t a, b, c, d;
        if(x86_64::cpuid(0x1, a, b, c, d)){
            uint8_t id = ((b >> 24) & 0xFF);
            if(id == lapic->apic_id) bsp = true;
            else bsp = false;
        } else {
            // Assume not BSP
            bsp = false;
        }
        cpus.push_back({.lapic_id = lapic->apic_id, .x2apic = false, .bsp = bsp});
    } else {
        debug_printf(", Disabled");
    }
    debug_printf("\n");
}

void acpi::madt::parse_x2apic(uint8_t* item){
    auto* lapic = reinterpret_cast<acpi::madt_x2apic*>(item);

    debug_printf("[MADT]: Detected CPU: ACPI UID: %x, x2APIC id: %x", lapic->acpi_uid, lapic->x2apic_id);
    uint32_t flags = lapic->flags;
    if(bitops<uint32_t>::bit_test(flags, acpi::madt_lapic_flags_enabled) || (!bitops<uint32_t>::bit_test(flags, acpi::madt_lapic_flags_enabled) && bitops<uint32_t>::bit_test(flags, acpi::madt_lapic_flags_online_capable))){
        bool bsp;
        uint32_t a, b, c, d;
        if(x86_64::cpuid(0xB, a, b, c, d)){
            if(d == lapic->x2apic_id) bsp = true;
            else bsp = false;
        } else {
            // Assume not BSP
            bsp = false;
        }
        cpus.push_back({.lapic_id = lapic->x2apic_id, .x2apic = true, .bsp = bsp});
    } else {
        debug_printf(", Disabled");
    }
    debug_printf("\n");
}

void acpi::madt::parse_ioapic(uint8_t* item){
    auto* ioapic = reinterpret_cast<acpi::madt_ioapic*>(item);

    debug_printf("[MADT]: Detected IOAPIC: base: %x, GSI base: %x\n", ioapic->ioapic_addr, ioapic->gsi_base);

    auto addr = ioapic->ioapic_addr;
    auto gsi_base = ioapic->gsi_base;
    this->ioapics.push_back({addr, gsi_base});
}

void acpi::madt::parse_iso(uint8_t* item){
    auto* iso = reinterpret_cast<acpi::madt_gsi_override*>(item);

    debug_printf("[MADT]: Detected Interrupt Source Override: ISA IRQ: %x, GSI: %x, flags: %x\n", iso->source, iso->gsi, iso->flags);

    this->isos.push_back({.source = iso->source, .gsi = iso->gsi, .flags = iso->flags}); // constructs an x86_64::apic::interrupt_override
}

void acpi::madt::parse(){
    uint32_t flags = this->table->flags;
    if(bitops<uint32_t>::bit_test(flags, acpi::flags_pc_at_compatibility)){
        this->legacy_pic = true;
    } else {
        this->legacy_pic = false;
    }

    size_t table_size = (this->table->header.length - sizeof(acpi::madt_header));
    uint64_t list = reinterpret_cast<uint64_t>(this->table) + sizeof(acpi::madt_header);
    uint64_t offset = 0;
    while((list + offset) < (list + table_size)){
        uint8_t* item = reinterpret_cast<uint8_t*>((list + offset));
        uint8_t type = item[0]; // Type
        switch (type)
        {
        case acpi::type_lapic:
            this->parse_lapic(item);
            break;

        case acpi::type_ioapic:
            this->parse_ioapic(item);
            break;

        case acpi::type_interrupt_source_override:
            this->parse_iso(item);
            break;

        case acpi::type_x2apic:
            this->parse_x2apic(item);
            break;
        
        case acpi::type_nmi_source:
            break;
        
        default:
            debug_printf("[MADT]: Unknown table type: %x\n", type);
            break;
        }
        offset += item[1]; // Length
    }
}



void acpi::madt::get_cpus(types::linked_list<smp::cpu_entry>& cpus){
    for(const auto& a : this->cpus) cpus.push_back(a);
}

void acpi::madt::get_ioapics(types::linked_list<std::pair<uint64_t, uint32_t>>& ioapics){
    for(const auto& a : this->ioapics) ioapics.push_back(a);
}

void acpi::madt::get_interrupt_overrides(types::linked_list<x86_64::apic::interrupt_override>& isos){
    for(const auto& a : this->isos) isos.push_back(a);
}
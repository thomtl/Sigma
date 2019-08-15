#include <Sigma/acpi/acpi.h>

extern "C" {
#include <lai/core.h>
#include <lai/helpers/sci.h>
#include <lai/helpers/pm.h>
#include <lai/drivers/ec.h>
}


auto acpi_tables = types::linked_list<uint64_t>();

static bool do_checksum(acpi::sdt_header* header){
    uint8_t sum = 0;

    for(size_t i = 0; i < header->length; i++) sum += ((uint8_t*)header)[i];

    return (sum == 0) ? (true) : (false);
}

acpi::table* acpi::get_table(const char* signature, uint64_t index){
    if(signature == acpi::dsdt_signature){
        acpi::fadt* fadt = reinterpret_cast<acpi::fadt*>(acpi::get_table(acpi::fadt_signature));
        uint64_t dsdt_phys_addr = 0;

        if(IS_CANONICAL(fadt->x_dsdt)) dsdt_phys_addr = fadt->x_dsdt;
        else dsdt_phys_addr = fadt->dsdt;

        uint64_t dsdt_addr = (dsdt_phys_addr + KERNEL_PHYSICAL_VIRTUAL_MAPPING_BASE);

        mm::vmm::kernel_vmm::get_instance().map_page(dsdt_phys_addr, dsdt_addr, map_page_flags_present | map_page_flags_cache_disable | map_page_flags_no_execute);
        for (size_t i = 1; i < ((((acpi::sdt_header*)dsdt_addr)->length / mm::pmm::block_size) + 1); i++){
            mm::vmm::kernel_vmm::get_instance().map_page((dsdt_phys_addr + (mm::pmm::block_size * i)), (dsdt_addr + (mm::pmm::block_size * i)), map_page_flags_present | map_page_flags_cache_disable | map_page_flags_no_execute);
        }

        return reinterpret_cast<acpi::table*>(dsdt_addr);
    }

    uint64_t curr = 0;
    for(auto table : acpi_tables){
        acpi::sdt_header* header = reinterpret_cast<acpi::sdt_header*>(table);

        if((signature[0] == header->signature[0]) && (signature[1] == header->signature[1]) && (signature[2] == header->signature[2]) && (signature[3] == header->signature[3])){
            if(curr != index) curr++;
            else return reinterpret_cast<acpi::table*>(header);
        }
    }

    debug_printf("[ACPI]: Couldn't find table %c%c%c%c, index: %d\n", signature[0], signature[1], signature[2], signature[3], index);
    return nullptr;
}

acpi::table* acpi::get_table(const char* signature){
    if(signature == acpi::dsdt_signature){
        acpi::fadt* fadt = reinterpret_cast<acpi::fadt*>(acpi::get_table(acpi::fadt_signature));
        uint64_t dsdt_phys_addr = 0;

        if(fadt->x_dsdt != 0) dsdt_phys_addr = fadt->x_dsdt;
        else dsdt_phys_addr = fadt->dsdt;

        uint64_t dsdt_addr = (dsdt_phys_addr + KERNEL_PHYSICAL_VIRTUAL_MAPPING_BASE);

        mm::vmm::kernel_vmm::get_instance().map_page(dsdt_phys_addr, dsdt_addr, map_page_flags_present | map_page_flags_cache_disable | map_page_flags_no_execute);
        for (size_t i = 1; i < ((((acpi::sdt_header*)dsdt_addr)->length / mm::pmm::block_size) + 1); i++){
            mm::vmm::kernel_vmm::get_instance().map_page((dsdt_phys_addr + (mm::pmm::block_size * i)), (dsdt_addr + (mm::pmm::block_size * i)), map_page_flags_present | map_page_flags_cache_disable | map_page_flags_no_execute);
        }

        return reinterpret_cast<acpi::table*>(dsdt_addr);
    }
    for(auto table : acpi_tables){
        acpi::sdt_header* header = reinterpret_cast<acpi::sdt_header*>(table);

        if((signature[0] == header->signature[0]) && (signature[1] == header->signature[1]) && (signature[2] == header->signature[2]) && (signature[3] == header->signature[3])){
            return reinterpret_cast<acpi::table*>(header);
        }
    }

    debug_printf("[ACPI]: Couldn't find table %c%c%c%c\n", signature[0], signature[1], signature[2], signature[3]);
    return nullptr;
}

uint16_t acpi::get_arch_boot_flags(){
    acpi::fadt* fadt = reinterpret_cast<acpi::fadt*>(reinterpret_cast<uint64_t>(acpi::get_table(acpi::fadt_signature)) + KERNEL_PHYSICAL_VIRTUAL_MAPPING_BASE);

    uint16_t flags = 0;

    #ifdef ARCH_X86_64
    flags = fadt->iapc_boot_arch;
    #elif ARCH_ARM
    flags = fadt->arm_boot_arch;
    #endif

    return flags;
}

void acpi::init(boot::boot_protocol* boot_protocol){
    FUNCTION_CALL_ONCE();

    acpi::rsdp* rsdp = reinterpret_cast<acpi::rsdp*>(boot_protocol->acpi_pointer);

    mm::vmm::kernel_vmm::get_instance().map_page(reinterpret_cast<uint64_t>(rsdp), (reinterpret_cast<uint64_t>(rsdp) + KERNEL_PHYSICAL_VIRTUAL_MAPPING_BASE), map_page_flags_present | map_page_flags_cache_disable | map_page_flags_no_execute);

    uint8_t rsdp_checksum = 0;
    for(size_t i = 0; i < sizeof(acpi::rsdp); i++) rsdp_checksum += ((uint8_t*)rsdp)[i];

    if(rsdp_checksum != 0){
        printf("[ACPI]: Failed RSDP checksum\n");
        return;
    }

    if(rsdp->revision > 0){
        debug_printf("[ACPI]: Detected version 2 or higher\n");
        acpi::xsdp* xsdp = reinterpret_cast<acpi::xsdp*>(rsdp);

        uint8_t xsdp_checksum = 0;
        for(size_t i = 0; i < sizeof(acpi::xsdp); i++) xsdp_checksum += ((uint8_t*)xsdp)[i];

        if(xsdp_checksum != 0){
            printf("[ACPI]: Failed XSDP checksum\n");
            return;
        }

        debug_printf("[ACPI]: Found XSDP: oem_id:%c%c%c%c%c%c, Revision: %d\n", xsdp->oem_id[0], xsdp->oem_id[1], xsdp->oem_id[2], xsdp->oem_id[3], xsdp->oem_id[4], xsdp->oem_id[5], xsdp->revision);
        

        mm::vmm::kernel_vmm::get_instance().map_page(xsdp->xsdt_address, (xsdp->xsdt_address + KERNEL_PHYSICAL_VIRTUAL_MAPPING_BASE), map_page_flags_present | map_page_flags_cache_disable | map_page_flags_no_execute);

        auto* xsdt = reinterpret_cast<acpi::xsdt*>(xsdp->xsdt_address + KERNEL_PHYSICAL_VIRTUAL_MAPPING_BASE);

        if(!do_checksum(reinterpret_cast<acpi::sdt_header*>(xsdt))){
            printf("[ACPI]: Failed XSDT checksum\n");
            return;
        }


        size_t entries = (xsdt->header.length - sizeof(xsdt->header)) / 8;

        for (size_t i = 0; i < entries; i++)
        {

            if(reinterpret_cast<uint64_t*>(xsdt->tables[i]) == nullptr) continue;

            uint64_t page_addr = xsdt->tables[i] & 0xFFFFFFFFFFFFF000;
            mm::vmm::kernel_vmm::get_instance().map_page((page_addr + 0x1000), ((page_addr + 0x1000) + KERNEL_PHYSICAL_VIRTUAL_MAPPING_BASE), map_page_flags_present | map_page_flags_cache_disable | map_page_flags_no_execute);

            mm::vmm::kernel_vmm::get_instance().map_page(xsdt->tables[i], (xsdt->tables[i] + KERNEL_PHYSICAL_VIRTUAL_MAPPING_BASE), map_page_flags_present | map_page_flags_cache_disable | map_page_flags_no_execute);

            auto* h = reinterpret_cast<acpi::sdt_header*>(xsdt->tables[i] + KERNEL_PHYSICAL_VIRTUAL_MAPPING_BASE);

            if(do_checksum(h)){
                debug_printf("[ACPI]: Found Table %c%c%c%c: oem_id:%c%c%c%c%c%c, Revision: %d\n", h->signature[0], h->signature[1], h->signature[2], h->signature[3], h->oem_id[0], h->oem_id[1], h->oem_id[2], h->oem_id[3], h->oem_id[4], h->oem_id[5], h->revision);

                acpi_tables.push_back(reinterpret_cast<uint64_t>(h));

            }

        }


    } else {
        debug_printf("[ACPI]: Found RSDP: oem_id:%c%c%c%c%c%c, Revision: %d\n", rsdp->oem_id[0], rsdp->oem_id[1], rsdp->oem_id[2], rsdp->oem_id[3], rsdp->oem_id[4], rsdp->oem_id[5], rsdp->revision);

        

        mm::vmm::kernel_vmm::get_instance().map_page(rsdp->rsdt_address, (rsdp->rsdt_address + KERNEL_PHYSICAL_VIRTUAL_MAPPING_BASE), map_page_flags_present | map_page_flags_cache_disable | map_page_flags_no_execute);

        auto* rsdt = reinterpret_cast<acpi::rsdt*>(rsdp->rsdt_address + KERNEL_PHYSICAL_VIRTUAL_MAPPING_BASE);

        if(!do_checksum(reinterpret_cast<acpi::sdt_header*>(rsdt))){
            printf("[ACPI]: Failed RSDT checksum\n");
            return;
        }

        size_t entries = (rsdt->header.length - sizeof(acpi::sdt_header)) / 4;
        for (size_t i = 0; i < entries; i++)
        {
            if(reinterpret_cast<uint64_t*>(rsdt->tables[i]) == nullptr) continue;
            mm::vmm::kernel_vmm::get_instance().map_page(rsdt->tables[i], (rsdt->tables[i] + KERNEL_PHYSICAL_VIRTUAL_MAPPING_BASE), map_page_flags_present | map_page_flags_cache_disable | map_page_flags_no_execute);
            auto* h = reinterpret_cast<acpi::sdt_header*>(rsdt->tables[i] + KERNEL_PHYSICAL_VIRTUAL_MAPPING_BASE);
            if(do_checksum(h)){
                debug_printf("[ACPI]: Found Table %c%c%c%c: oem_id:%c%c%c%c%c%c, Revision: %d\n", h->signature[0], h->signature[1], h->signature[2], h->signature[3], h->oem_id[0], h->oem_id[1], h->oem_id[2], h->oem_id[3], h->oem_id[4], h->oem_id[5], h->revision);

                acpi_tables.push_back(reinterpret_cast<uint64_t>(h));
            }

        }

    }
    //lai_enable_tracing(1);

    lai_set_acpi_revision(rsdp->revision);

    lai_create_namespace();

    acpi::init_ec();
}

static void acpi_sci_handler(x86_64::idt::idt_registers* regs){
    UNUSED(regs);
    uint16_t event = lai_get_sci_event();
    if(event & ACPI_POWER_BUTTON){
        debug_printf("[ACPI]: Requested ACPI shutdown at tsc: %x\n", x86_64::read_tsc());
        lai_enter_sleep(5); // S5 is off
    } else {
        printf("[ACPI]: Unknown SCI event: %x\n", event);
    }
}

void acpi::init_sci(acpi::madt& madt){
    FUNCTION_CALL_ONCE();
    auto* fadt = reinterpret_cast<acpi::fadt*>(acpi::get_table(acpi::fadt_signature));
    uint16_t sci_int = fadt->sci_int;

    if(!madt.supports_legacy_pic()){
        // FADT->SCI_INT contains a GSI, map ourselves

        // ACPI spec states that is it a sharable, level, active low interrupt
        x86_64::apic::ioapic::set_entry(sci_int, (sci_int + 0x20), x86_64::apic::ioapic_delivery_modes::FIXED, x86_64::apic::ioapic_destination_modes::PHYSICAL, ((1 << 13) | (1 << 15)), smp::cpu::get_current_cpu()->lapic_id); // Target the BSP
    }

    x86_64::idt::register_interrupt_handler((sci_int + 0x20), acpi_sci_handler, true);
    x86_64::apic::ioapic::unmask_irq(sci_int);

    lai_enable_acpi(1); // argument is interrupt mode, 1 = APIC, 0 = PIC, 2 = SAPIC, SAPIC doesn't even exist on x86_64 only on IA64(Itanium)    

    debug_printf("[ACPI]: Enabled SCI on IRQ: %x\n", sci_int);
}

void acpi::init_ec(){
    FUNCTION_CALL_ONCE();
    LAI_CLEANUP_STATE lai_state_t state;
    lai_init_state(&state);
 
    LAI_CLEANUP_VAR lai_variable_t pnp_id = {};
    constexpr const char* ec_pnp_id = ACPI_EC_PNP_ID;
    lai_eisaid(&pnp_id, const_cast<char*>(ec_pnp_id));
 
    struct lai_ns_iterator it = {};
    lai_nsnode_t *node;
    while((node = lai_ns_iterate(&it))){
        if(lai_check_device_pnp_id(node, &pnp_id, &state)) // This is not an EC
            continue;
 
        // Found one
        struct lai_ec_driver *driver = new struct lai_ec_driver; // Dynamically allocate the memory since -
        lai_init_ec(node, driver);                               // we dont know how many ECs there could be
 
        struct lai_ns_child_iterator child_it = LAI_NS_CHILD_ITERATOR_INITIALIZER(node);
        lai_nsnode_t *child_node;
        while((child_node = lai_ns_child_iterate(&child_it))){
            if(lai_ns_get_node_type(child_node) == LAI_NODETYPE_OPREGION)
                lai_ns_override_opregion(child_node, &lai_ec_opregion_override, driver);
        }
    }
}
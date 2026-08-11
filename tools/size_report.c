#include <stdio.h>
#include "wcm/abi.h"
#include "wcm/wcm.h"

int main(void) {
    wcm_build_info_t build;
    wcm_get_build_info(&build);
    printf("WCM Core %s size report\n", build.version);
    printf("profile=%u\n", (unsigned)build.profile);
    printf("pointer_bits=%u\n", (unsigned)build.pointer_bits);
    printf("wcm_value_t=%zu\n", sizeof(wcm_value_t));
    printf("wcm_worldstamp_t=%zu\n", sizeof(wcm_worldstamp_t));
    printf("wcm_witness_t=%zu\n", sizeof(wcm_witness_t));
    printf("wcm_intent_t=%zu\n", sizeof(wcm_intent_t));
    printf("wcm_snapshot_t=%zu\n", sizeof(wcm_snapshot_t));
    printf("wcm_module_runtime_t=%zu\n", sizeof(wcm_module_runtime_t));
    printf("wcm_rebind_runtime_t=%zu\n", sizeof(wcm_rebind_runtime_t));
    printf("wcm_witness_store_t=%zu\n", sizeof(wcm_witness_store_t));
    printf("wcm_ingress_t=%zu\n", sizeof(wcm_ingress_t));
    printf("wcm_capability_graph_t=%zu\n", sizeof(wcm_capability_graph_t));
    printf("wcm_event_record_t=%zu\n", sizeof(wcm_event_record_t));
    printf("wcm_health_snapshot_t=%zu\n", sizeof(wcm_health_snapshot_t));
    printf("wcm_anchor_slot_t=%zu\n", sizeof(wcm_anchor_slot_t));
    printf("runtime_storage_required=%zu\n", wcm_runtime_storage_required());
    printf("runtime_storage_reserved=%u\n", (unsigned)build.runtime_storage_bytes);
    return 0;
}

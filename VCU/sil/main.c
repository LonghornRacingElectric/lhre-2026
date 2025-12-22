#include <stdio.h>
#include <stdlib.h>

#include "fmilib.h"

int main(int argc, char* argv[]) {
    const char* fmu_path =
        "/home/dhairyagupta/Downloads/TestMF5p2RigidVehicle.fmu";
    const char* temp_dir = "/tmp/temp_fmu_unpack_dir";

    printf("Tryna work\n");

    // 1. Setup Callbacks (Memory and Logging)
    jm_callbacks* callbacks = jm_get_default_callbacks();

    printf("Got callbacks\n");

    // 2. Create Import Context
    fmi_import_context_t* context = fmi_import_allocate_context(callbacks);

    printf("Imported callback context\n");

    // 3. Unzip and Parse the FMU
    // This extracts the FMU to temp_dir and parses modelDescription.xml
    fmi_version_enu_t version =
        fmi_import_get_fmi_version(context, fmu_path, temp_dir);

    printf("Loaded FMU in\n");

    if (version != fmi_version_2_0_enu) {
        printf("Example supports FMI 2.0 only.\n");
        return 1;
    }
}
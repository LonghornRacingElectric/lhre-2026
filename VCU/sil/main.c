#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "fmilib.h"

#ifdef _WIN32
const char* temp_dir = "C:\\tmp\\temp_fmu_unpack_dir";
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#include <sys/types.h>
const char* temp_dir = "/tmp/temp_fmu_unpack_dir";
#define MKDIR(path) mkdir(path, 0777)
#endif

// So we can guarnatee that a directory exists
// before we attempt to unzip the FMU into it.
int make_directory(const char* path) {
    int result = MKDIR(path);
    if (result == 0) {
        return 0;
    } else {
        if (errno == EEXIST) {
            return 0;
        }
        return -1;
    }
}

int main(int argc, char* argv[]) {
    const char* fmu_path = "<path to FMU>";

    if (make_directory(temp_dir) == 0) {
        printf("Directory ready: %s\n", temp_dir);
    } else {
        perror("Failed to create directory");
    }

    // Simulation Settings
    fmi2_real_t t_start = 0.0;
    fmi2_real_t t_stop = 10.0;
    fmi2_real_t step_size = 0.1;
    // --------------------------

    printf("Tryna work\n");

    // 1. Setup Callbacks
    jm_callbacks* callbacks = jm_get_default_callbacks();

    // 2. Create Import Context
    fmi_import_context_t* context = fmi_import_allocate_context(callbacks);

    // 3. Unzip and Check Version
    printf("Unzipping FMU to %s...\n", temp_dir);
    fmi_version_enu_t version =
        fmi_import_get_fmi_version(context, fmu_path, temp_dir);

    if (version != fmi_version_2_0_enu) {
        printf("Error: This example supports FMI 2.0 only.\n");
        fmi_import_free_context(context);
        return 1;
    }
    printf("FMU is FMI 2.0. Parsing XML...\n");

    // 4. Parse the Model Description (XML)
    // We pass the directory where the FMU was unzipped
    fmi2_import_t* fmu = fmi2_import_parse_xml(context, temp_dir, 0);

    if (!fmu) {
        printf("Error: Could not parse modelDescription.xml\n");
        fmi_import_free_context(context);
        return 1;
    }

    // 5. Check for Co-Simulation Capability
    if (fmi2_import_get_fmu_kind(fmu) == fmi2_fmu_kind_me) {
        printf(
            "Error: This code only supports Co-Simulation, but FMU is Model "
            "Exchange.\n");
        fmi2_import_free(fmu);
        fmi_import_free_context(context);
        return 1;
    }

    // 6. Load the Binary (DLL/Shared Object)
    // The library handles loading the correct binary for your OS automatically
    printf("Loading binary...\n");
    jm_status_enu_t jm_status =
        fmi2_import_create_dllfmu(fmu, fmi2_fmu_kind_cs, NULL);
    if (jm_status == jm_status_error) {
        printf("Error: Could not create DLL FMU.\n");
        fmi2_import_free(fmu);
        fmi_import_free_context(context);
        return 1;
    }

    // 7. Instantiate the FMU
    printf("Instantiating...\n");
    const char* instance_name = "TestRigidVehicle";
    // fmi2_import_instantiate(fmu, instanceName, fmuType, resourcePath,
    // visible) passing NULL for resourcePath lets fmilib resolve it
    // automatically
    jm_status =
        fmi2_import_instantiate(fmu, instance_name, fmi2_fmu_kind_cs, NULL, 0);

    if (jm_status == jm_status_error) {
        printf("Error: Instantiation failed.\n");
        fmi2_import_free_instance(fmu);
        return 1;
    }

    // 8. Setup Experiment
    // (fmu, toleranceDefined, tolerance, startTime, stopTimeDefined, stopTime)
    fmi2_import_setup_experiment(fmu, fmi2_true, 1e-4, t_start, fmi2_true,
                                 t_stop);

    // 9. Initialization Mode
    printf("Initializing...\n");
    fmi2_import_enter_initialization_mode(fmu);
    // (Optional: Set start values for variables here using
    // fmi2_import_set_real/integer)
    fmi2_import_exit_initialization_mode(fmu);

    // 10. Simulation Loop
    printf("Starting simulation from %.1f to %.1f...\n", t_start, t_stop);
    fmi2_real_t current_time = t_start;

    while (current_time < t_stop) {
        // fmi2_import_do_step(fmu, currentCommunicationPoint, stepSize,
        // noSetFMUStatePriorToCurrentPoint)
        fmi2_status_t fmi_status =
            fmi2_import_do_step(fmu, current_time, step_size, fmi2_true);

        if (fmi_status != fmi2_status_ok) {
            printf(
                "Error: Simulation stopped early at time %f with status %d\n",
                current_time, fmi_status);
            break;
        }

        // Advance time
        current_time += step_size;

        // (Optional: Read outputs here using fmi2_import_get_real)
        printf("Time: %.2f\n", current_time);
    }

    printf("Simulation finished successfully.\n");

    // 11. Cleanup
    fmi2_import_terminate(fmu);
    fmi2_import_free_instance(fmu);
    fmi2_import_destroy_dllfmu(fmu);
    fmi2_import_free(fmu);
    fmi_import_free_context(context);

    return 0;
}
from fmpy import read_model_description, extract
from fmpy.fmi2 import FMU2Slave


class FMU:
    def __init__(self, path, name):
        md = read_model_description(path)
        unzipdir = extract(path)

        self.fmu = FMU2Slave(
            guid=md.guid,
            unzipDirectory=unzipdir,
            modelIdentifier=md.coSimulation.modelIdentifier,
            instanceName=name,
        )

        self.fmu.instantiate()
        self.fmu.setupExperiment(startTime=0.0)
        self.fmu.enterInitializationMode()
        self.fmu.exitInitializationMode()

        self.vr = {v.name: v.valueReference for v in md.modelVariables}
        self.t = 0.0

    def get(self, name):
        return self.fmu.getReal([self.vr[name]])[0]

    def set(self, name, val):
        self.fmu.setReal([self.vr[name]], [val])

    def get_vec3(self, base):
        return [
            self.fmu.getReal([self.vr[f"{base}[1]"]])[0],
            self.fmu.getReal([self.vr[f"{base}[2]"]])[0],
            self.fmu.getReal([self.vr[f"{base}[3]"]])[0],
        ]

    def step(self, dt):
        self.fmu.doStep(self.t, dt)
        self.t += dt
    
    def terminate(self):
        try:
            self.fmu.terminate()
        except:
            pass

        try:
            self.fmu.freeInstance()
        except:
            pass
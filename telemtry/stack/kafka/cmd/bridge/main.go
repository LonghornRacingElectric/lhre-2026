package main

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"net"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"

	"github.com/IBM/sarama"
	pb "github.com/LonghornRacingElectric/lhre-2026/telemtry/stack/kafka/proto/bridge"
	sensor "github.com/LonghornRacingElectric/lhre-2026/telemtry/stack/kafka/proto/sensor"
	"google.golang.org/grpc"
	"google.golang.org/protobuf/proto"
)

type SensorMessage struct {
	Payload []byte
	CarType string
}

var (
	rawDataChan     = make(chan SensorMessage, 10000)
	grafanaDataChan = make(chan SensorMessage, 10000)
)

type bridgeServer struct {
	pb.UnimplementedBridgeServiceServer
}

func (s *bridgeServer) SendSensorData(ctx context.Context, req *pb.SensorDataRequest) (*pb.SensorDataResponse, error) {
	log.Printf("Received SensorDataRequest: CarType=%s, PayloadSize=%d", req.CarType, len(req.Payload))
	msg := SensorMessage{
		Payload: req.Payload,
		CarType: req.CarType,
	}

	// Non-blocking send to both channels
	select {
	case rawDataChan <- msg:
	default:
		log.Println("Warning: rawDataChan full, dropping message")
	}

	select {
	case grafanaDataChan <- msg:
	default:
		log.Println("Warning: grafanaDataChan full, dropping message")
	}

	return &pb.SensorDataResponse{Success: true, Message: "queued"}, nil
}

func getKafkaBroker() string {
	if broker := os.Getenv("KAFKA_BROKER"); broker != "" {
		return broker
	}
	if os.Getenv("IN_DOCKER") != "" {
		return "kafka:9092"
	}
	return "localhost:29092"
}

func newKafkaProducer() (sarama.SyncProducer, error) {
	config := sarama.NewConfig()
	config.Producer.Return.Successes = true
	config.Producer.RequiredAcks = sarama.WaitForLocal
	config.Producer.Retry.Max = 3

	broker := getKafkaBroker()
	return sarama.NewSyncProducer([]string{broker}, config)
}

// newKafkaAdmin creates a ClusterAdmin for topic management
func newKafkaAdmin() (sarama.ClusterAdmin, error) {
    config := sarama.NewConfig()
    config.Version = sarama.V3_6_0_0 // Use latest stable version
    config.Admin.Timeout = 10 * time.Second
    broker := getKafkaBroker()
    return sarama.NewClusterAdmin([]string{broker}, config)
}

func strPtr(s string) *string { return &s }

// ensureTopicExists creates the topic if it doesn't already exist
func ensureTopicExists(admin sarama.ClusterAdmin, topic string) error {
    // Retry logic for listing topics
    var topics map[string]sarama.TopicDetail
    var err error
    maxRetries := 5
    
    for i := 0; i < maxRetries; i++ {
        topics, err = admin.ListTopics()
        if err == nil {
            break
        }
        log.Printf("Attempt %d/%d: Failed to list topics: %v", i+1, maxRetries, err)
        time.Sleep(2 * time.Second)
    }
    
    if err != nil {
        return fmt.Errorf("failed to list topics after %d retries: %w", maxRetries, err)
    }
    
    if _, ok := topics[topic]; ok {
        log.Printf("Topic %s already exists", topic)
        return nil
    }

    log.Printf("Creating topic %s...", topic)
    detail := &sarama.TopicDetail{
        NumPartitions:     1,
        ReplicationFactor: 1,
        ConfigEntries: map[string]*string{
            "retention.ms":   strPtr("604800000"), // 7 days
            "cleanup.policy": strPtr("delete"),
        },
    }
    
    err = admin.CreateTopic(topic, detail, false)
    if err != nil {
        return fmt.Errorf("failed to create topic %s: %w", topic, err)
    }
    
    log.Printf("Successfully created topic %s", topic)
    
    // Verify topic was created
    time.Sleep(1 * time.Second)
    topics, err = admin.ListTopics()
    if err != nil {
        log.Printf("Warning: Could not verify topic creation: %v", err)
    } else if _, ok := topics[topic]; ok {
        log.Printf("Verified: Topic %s exists in Kafka", topic)
    } else {
        log.Printf("Warning: Topic %s not found after creation", topic)
    }
    
    return nil
}

// defaultSeedMap returns a zero-valued map for the given car type, populated
// via the same protobuf -> map converters used in the hot path. This ensures
// every column Grafana expects is present (not just time/packet_id) so that
// dashboards render on an empty topic.
func defaultSeedMap(carType string) map[string]interface{} {
	var m map[string]interface{}
	switch strings.ToLower(strings.TrimSpace(carType)) {
	case "angelique":
		m = angeliqueToMap(&sensor.AngeliqueSensorData{
			Dynamics:    &sensor.AngeliqueDynamics{},
			Controls:    &sensor.AngeliqueControls{},
			Pack:        &sensor.AngeliquePack{},
			Diagnostics: &sensor.AngeliqueDiagnostics{},
			Thermal:     &sensor.AngeliqueThermal{},
		})
	case "orion":
		m = orionToMap(&sensor.OrionSensorData{
			Dynamics:        &sensor.OrionDynamics{},
			Controls:        &sensor.OrionControls{},
			Pack:            &sensor.OrionPack{},
			DiagnosticsHigh: &sensor.OrionDiagnosticsHigh{},
			DiagnosticsLow:  &sensor.OrionDiagnosticsLow{},
			Thermal:         &sensor.OrionThermal{},
			BoardStatus:     &sensor.OrionBoardStatus{},
		})
	default:
		m = nightwatchToMap(&sensor.SensorData{
			Dynamics:       &sensor.Dynamics{},
			Controls:       &sensor.Controls{},
			Pack:           &sensor.Pack{},
			DiagnosticsLow: &sensor.DiagnosticsLow{},
			Thermal:        &sensor.Thermal{},
		})
	}
	m["time"] = time.Now().UnixMilli()
	m["packet_id"] = 0
	m["car_type"] = carType
	return m
}

// Init method for kafka grafana datasource
func seedTopic(producer sarama.SyncProducer, topic, carType string) {
	jsonData, err := json.Marshal(defaultSeedMap(carType))
	if err != nil {
		log.Printf("Failed to marshal seed data: %v", err)
		return
	}

	log.Printf("Sending seed message to topic %s: %s", topic, string(jsonData))

	msg := &sarama.ProducerMessage{
		Topic:     topic,
		Value:     sarama.ByteEncoder(jsonData),
		Timestamp: time.Now(),
	}
	partition, offset, err := producer.SendMessage(msg)
	if err != nil {
		log.Printf("Failed to seed topic %s: %v", topic, err)
	} else {
		log.Printf("Successfully seeded topic %s (partition: %d, offset: %d)", topic, partition, offset)
	}
}

// rawDataWorker publishes raw protobuf bytes to sensor_data topic
func rawDataWorker(producer sarama.SyncProducer) {
	for msg := range rawDataChan {
		kafkaMsg := &sarama.ProducerMessage{
			Topic: "sensor_data",
			Value: sarama.ByteEncoder(msg.Payload),
			Headers: []sarama.RecordHeader{
				{Key: []byte("car_type"), Value: []byte(msg.CarType)},
			},
		}

		_, _, err := producer.SendMessage(kafkaMsg)
		if err != nil {
			log.Printf("Error sending to sensor_data: %v", err)
		}
	}
}

// grafanaDataWorker deserializes protobuf and publishes JSON to car-specific grafana_data topics
func grafanaDataWorker(producer sarama.SyncProducer) {
	for msg := range grafanaDataChan {
		jsonData, err := deserializeToJSON(msg.Payload, msg.CarType)
		if err != nil {
			log.Printf("Error deserializing protobuf for car %s: %v", msg.CarType, err)
			continue
		}

		normalizedCarType := strings.ToLower(strings.TrimSpace(msg.CarType))
		if normalizedCarType == "" {
			normalizedCarType = "unknown"
			log.Printf("Warning: Empty CarType received, using 'grafana_data_unknown' topic")
		}
		topic := fmt.Sprintf("grafana_data_%s", normalizedCarType)

		kafkaMsg := &sarama.ProducerMessage{
			Topic: topic,
			Value: sarama.ByteEncoder(jsonData),
			Headers: []sarama.RecordHeader{
				{Key: []byte("car_type"), Value: []byte(msg.CarType)},
			},
		}

		partition, offset, err := producer.SendMessage(kafkaMsg)
		if err != nil {
			log.Printf("Error sending to topic %s: %v", topic, err)
		} else {
			log.Printf("Successfully sent message to topic %s (partition: %d, offset: %d)", topic, partition, offset)
		}
	}
}

func deserializeToJSON(payload []byte, carType string) ([]byte, error) {
	var data map[string]interface{}
	normalizedCarType := strings.ToLower(strings.TrimSpace(carType))

	switch normalizedCarType {
	case "angelique":
		msg := &sensor.AngeliqueSensorData{}
		if err := proto.Unmarshal(payload, msg); err != nil {
			return nil, fmt.Errorf("failed to unmarshal Angelique payload: %w", err)
		}
		data = angeliqueToMap(msg)
	case "orion":
		msg := &sensor.OrionSensorData{}
		if err := proto.Unmarshal(payload, msg); err != nil {
			return nil, fmt.Errorf("failed to unmarshal Orion payload (size %d): %w", len(payload), err)
		}
		// Debug: Log if sub-blocks are present
		if msg.Controls == nil {
			log.Println("Debug: Orion Controls sub-block is MISSING in protobuf")
		} else {
			log.Printf("Debug: Orion Controls sub-block PRESENT. Apps1V: %v", msg.Controls.Apps1V)
		}
		data = orionToMap(msg)
	case "nightwatch":
		msg := &sensor.SensorData{}
		if err := proto.Unmarshal(payload, msg); err != nil {
			return nil, fmt.Errorf("failed to unmarshal Nightwatch payload: %w", err)
		}
		data = nightwatchToMap(msg)
	default:
		log.Printf("Warning: Unknown car type '%s', falling back to Nightwatch unmarshaling", carType)
		msg := &sensor.SensorData{}
		if err := proto.Unmarshal(payload, msg); err != nil {
			return nil, err
		}
		data = nightwatchToMap(msg)
	}

	data["car_type"] = carType
	return json.Marshal(data)
}

// statsFromInt32Slice returns average, min and max for a slice of int32.
// If the slice is empty the returned avg will be 0 and min/max will be 0.
func statsFromInt32Slice(vals []int32) (avg float64, min int32, max int32) {
	if len(vals) == 0 {
		return 0, 0, 0
	}
	var sum int64
	min = vals[0]
	max = vals[0]
	for _, v := range vals {
		sum += int64(v)
		if v < min {
			min = v
		}
		if v > max {
			max = v
		}
	}
	avg = float64(sum) / float64(len(vals))
	return avg, min, max
}

// statsFromFloat32Slice returns average, min and max for a slice of float32.
// If the slice is empty the returned avg will be 0 and min/max will be 0.
func statsFromFloat32Slice(vals []float32) (avg float64, min float32, max float32) {
	if len(vals) == 0 {
		return 0, 0, 0
	}
	var sum float64
	min = vals[0]
	max = vals[0]
	for _, v := range vals {
		sum += float64(v)
		if v < min {
			min = v
		}
		if v > max {
			max = v
		}
	}
	avg = sum / float64(len(vals))
	return avg, min, max
}

func nightwatchToMap(msg *sensor.SensorData) map[string]interface{} {
	m := make(map[string]interface{})
	m["time"] = msg.Time
	m["packet_id"] = msg.PacketId

	if d := msg.Dynamics; d != nil {
		m["steer_col_angle"] = d.SteerColAngle
		m["fl_steer_angle"] = d.FlSteerAngle
		m["fr_steer_angle"] = d.FrSteerAngle
		m["fl_sprung_accel"] = d.FlSprungAccel
		m["fr_sprung_accel"] = d.FrSprungAccel
		m["bl_sprung_accel"] = d.BlSprungAccel
		m["br_sprung_accel"] = d.BrSprungAccel
		m["fl_unsprung_accel"] = d.FlUnsprungAccel
		m["fr_unsprung_accel"] = d.FrUnsprungAccel
		m["bl_unsprung_accel"] = d.BlUnsprungAccel
		m["br_unsprung_accel"] = d.BrUnsprungAccel
		m["fl_sprung_ang_rate"] = d.FlSprungAngRate
		m["fr_sprung_ang_rate"] = d.FrSprungAngRate
		m["bl_sprung_ang_rate"] = d.BlSprungAngRate
		m["br_sprung_ang_rate"] = d.BrSprungAngRate
		m["cent_mass_accel"] = d.CentMassAccel
		m["cent_mass_ang_rate"] = d.CentMassAngRate
		m["flw_speed"] = d.FlwSpeed
		m["frw_speed"] = d.FrwSpeed
		m["blw_speed"] = d.BlwSpeed
		m["brw_speed"] = d.BrwSpeed
		m["fl_ride_height"] = d.FlRideHeight
		m["fr_ride_height"] = d.FrRideHeight
		m["bl_ride_height"] = d.BlRideHeight
		m["br_ride_height"] = d.BrRideHeight
		m["fl_strain_gauge_v"] = d.FlStrainGaugeV
		m["fr_strain_gauge_v"] = d.FrStrainGaugeV
		m["bl_strain_gauge_v"] = d.BlStrainGaugeV
		m["br_strain_gauge_v"] = d.BrStrainGaugeV
		m["fl_pushrod_stress"] = d.FlPushrodStress
		m["fr_pushrod_stress"] = d.FrPushrodStress
		m["bl_pushrod_stress"] = d.BlPushrodStress
		m["br_pushrod_stress"] = d.BrPushrodStress
		m["fl_spring_displace"] = d.FlSpringDisplace
		m["fr_spring_displace"] = d.FrSpringDisplace
		m["bl_spring_displace"] = d.BlSpringDisplace
		m["br_spring_displace"] = d.BrSpringDisplace
		m["dash_speed"] = d.DashSpeed
		m["f_gps"] = d.FGps
		m["b_gps"] = d.BGps
		m["f_gps_velocity"] = d.FGpsVelocity
		m["b_gps_velocity"] = d.BGpsVelocity
		m["f_gps_heading"] = d.FGpsHeading
		m["b_gps_heading"] = d.BGpsHeading
		if len(d.FGps) >= 2 {
			m["latitude"] = d.FGps[0]
			m["longitude"] = d.FGps[1]
		}
	}

	if c := msg.Controls; c != nil {
		m["apps1_v"] = c.Apps1V
		m["apps2_v"] = c.Apps2V
		m["apps1_t"] = c.Apps1T
		m["apps2_t"] = c.Apps2T
		m["accel_pedal_t"] = c.AccelPedalT
		m["bpps1_v"] = c.Bpps1V
		m["bpps2_v"] = c.Bpps2V
		m["bpps1_t"] = c.Bpps1T
		m["bpps2_t"] = c.Bpps2T
		m["brake_pedal_t"] = c.BrakePedalT
		m["bse1_v"] = c.Bse1V
		m["bse2_v"] = c.Bse2V
		m["bse3_v"] = c.Bse3V
		m["brake_pressure_f"] = c.BrakePressureF
		m["brake_pressure_rbll"] = c.BrakePressureRbll
		m["brake_pressure_rall"] = c.BrakePressureRall
		m["brake_bias"] = c.BrakeBias
	}

	if p := msg.Pack; p != nil {
		m["hv_pack_v"] = p.HvPackV
		m["hv_tractive_v"] = p.HvTractiveV
		m["hv_c"] = p.HvC
		m["lv_v"] = p.LvV
		m["lv_c"] = p.LvC
		m["contactor_state"] = p.ContactorState
		m["avg_cell_v"] = p.AvgCellV
		m["avg_cell_temp"] = p.AvgCellTemp
	}

	if dh := msg.DiagnosticsHigh; dh != nil {
		m["apps1_disconnect"] = dh.Apps1Disconnect
		m["apps2_disconnect"] = dh.Apps2Disconnect
		m["apps1_out_range"] = dh.Apps1OutRange
		m["apps2_out_range"] = dh.Apps2OutRange
		m["apps_mismatch"] = dh.AppsMismatch
		m["apps_implause"] = dh.AppsImplause
		m["bpps1_disconnect"] = dh.Bpps1Disconnect
		m["bpps2_disconnect"] = dh.Bpps2Disconnect
		m["bpps1_out_range"] = dh.Bpps1OutRange
		m["bpps2_out_range"] = dh.Bpps2OutRange
		m["bpps_mismatch"] = dh.BppsMismatch
		m["bse1_disconnect"] = dh.Bse1Disconnect
		m["bse2_disconnect"] = dh.Bse2Disconnect
		m["bse1_out_range"] = dh.Bse1OutRange
		m["bse2_out_range"] = dh.Bse2OutRange
	}

	if dl := msg.DiagnosticsLow; dl != nil {
		m["batt_over_c"] = dl.BattOverC
		m["cell_over_v"] = dl.CellOverV
		m["cell_under_v"] = dl.CellUnderV
		m["cell_open_wire"] = dl.CellOpenWire
		m["cell_damaged"] = dl.CellDamaged
		m["thermistor_damaged"] = dl.ThermistorDamaged
		m["bmb_comm_error"] = dl.BmbCommError
		m["imd_gnd_isolation_error"] = dl.ImdGndIsolationError
		m["tractive_contactor_error"] = dl.TractiveContactorError
		m["precharge_fail"] = dl.PrechargeFail
		m["cells_v_balanced"] = dl.CellsVBalanced
		m["cell_min_v"] = dl.CellMinV
		m["cell_max_v"] = dl.CellMaxV
		m["batt_v"] = dl.BattV
		m["batt_c"] = dl.BattC
		m["hv_soc"] = dl.HvSoc
		m["shutdown_leg1"] = dl.ShutdownLeg1
		m["shutdown_leg2"] = dl.ShutdownLeg2
		m["shutdown_leg3"] = dl.ShutdownLeg3
		m["shutdown_leg4"] = dl.ShutdownLeg4
		m["shutdown_leg5"] = dl.ShutdownLeg5
		m["shutdown_leg6"] = dl.ShutdownLeg6
		m["shutdown_leg7"] = dl.ShutdownLeg7
		m["shutdown_leg8"] = dl.ShutdownLeg8
		m["shutdown_leg9"] = dl.ShutdownLeg9
		m["shutdown_leg10"] = dl.ShutdownLeg10
		m["shutdown_leg11"] = dl.ShutdownLeg11
		m["shutdown_leg12"] = dl.ShutdownLeg12
		m["cells_temps"] = dl.CellsTemps
		m["cells_v"] = dl.CellsV
		if len(dl.CellsTemps) > 0 {
			avg, min, max := statsFromFloat32Slice(dl.CellsTemps)
			m["avg_cell_temp_stat"] = avg
			m["max_cell_temp"] = max
			m["min_cell_temp"] = min
		}
	}

	if t := msg.Thermal; t != nil {
		m["motor_loop_flow_rate"] = t.MotorLoopFlowRate
		m["motor_loop_motor_temp"] = t.MotorLoopMotorTemp
		m["motor_loop_inverter_temp"] = t.MotorLoopInverterTemp
		m["motor_loop_rad_temp"] = t.MotorLoopRadTemp
		m["motor_loop_rad_fan_speed"] = t.MotorLoopRadFanSpeed
		m["ambient_temp"] = t.AmbientTemp
		m["batt_loop_batt_temp"] = t.BattLoopBattTemp
		m["batt_loop_rad_temp"] = t.BattLoopRadTemp
		m["batt_loop_rad_fan_speed"] = t.BattLoopRadFanSpeed
		m["motor_temp"] = t.MotorTemp
		m["inverter_temp"] = t.InverterTemp
		m["bus_bar_temp1"] = t.BusBarTemp1
		m["bus_bar_temp2"] = t.BusBarTemp2
		m["bus_bar_temp3"] = t.BusBarTemp3
		m["precharge_r_temp"] = t.PrechargeRTemp
		m["discharge_r_temp"] = t.DischargeRTemp
		m["batt_over_temp"] = t.BattOverTemp
	}

	return m
}

func angeliqueToMap(msg *sensor.AngeliqueSensorData) map[string]interface{} {
	m := make(map[string]interface{})
	m["time"] = msg.Time
	m["packet_id"] = msg.PacketId

	if d := msg.Dynamics; d != nil {
		m["torque_request"] = d.TorqueRequest
		m["vcu_position"] = d.VcuPosition
		m["vcu_velocity"] = d.VcuVelocity
		m["vcu_accel"] = d.VcuAccel
		m["gps"] = d.Gps
		m["gps_velocity"] = d.GpsVelocity
		m["gps_heading"] = d.GpsHeading
		m["body1_accel"] = d.Body1Accel
		m["body2_accel"] = d.Body2Accel
		m["body3_accel"] = d.Body3Accel
		m["flw_accel"] = d.FlwAccel
		m["frw_accel"] = d.FrwAccel
		m["blw_accel"] = d.BlwAccel
		m["brw_accel"] = d.BrwAccel
		m["body1_gyro"] = d.Body1Gyro
		m["body2_gyro"] = d.Body2Gyro
		m["body3_gyro"] = d.Body3Gyro
		m["flw_speed"] = d.FlwSpeed
		m["frw_speed"] = d.FrwSpeed
		m["blw_speed"] = d.BlwSpeed
		m["brw_speed"] = d.BrwSpeed
		m["front_speed"] = (d.FlwSpeed + d.FrwSpeed) / 2
		m["rear_speed"] = (d.BlwSpeed + d.BrwSpeed) / 2
		m["inverter_v"] = d.InverterV
		m["inverter_c"] = d.InverterC
		m["inverter_rpm"] = d.InverterRpm
		m["inverter_torque"] = d.InverterTorque
		electricalPower := d.InverterV * float32(d.InverterC)
		mechanicalPower := float32(d.InverterRpm) * float32(d.InverterTorque) / 9.5490
		m["inverter_electrical_power"] = electricalPower
		m["inverter_mechanical_power"] = mechanicalPower
		if electricalPower != 0 {
			m["efficiency"] = mechanicalPower / electricalPower
		} else {
			m["efficiency"] = float32(0)
		}
		if len(d.Gps) >= 2 {
			m["latitude"] = d.Gps[0]
			m["longitude"] = d.Gps[1]
		}
	}

	if c := msg.Controls; c != nil {
		m["vcu_flags"] = c.VcuFlags
		m["vcu_flags_json"] = c.VcuFlagsJson
		m["apps1_v"] = c.Apps1V
		m["apps2_v"] = c.Apps2V
		m["bse1_v"] = c.Bse1V
		m["bse2_v"] = c.Bse2V
		m["sus1_v"] = c.Sus1V
		m["sus2_v"] = c.Sus2V
		m["steer_v"] = c.SteerV
	}

	if p := msg.Pack; p != nil {
		m["hv_pack_v"] = p.HvPackV
		m["hv_tractive_v"] = p.HvTractiveV
		m["hv_c"] = p.HvC
		m["lv_v"] = p.LvV
		m["lv_c"] = p.LvC
		m["contactor_state"] = p.ContactorState
		m["avg_cell_v"] = p.AvgCellV
		m["avg_cell_temp"] = p.AvgCellTemp
		m["hv_power"] = p.HvPackV * float32(p.HvC)
		m["lv_power"] = p.LvV * float32(p.LvC)
	}

	if dg := msg.Diagnostics; dg != nil {
		m["current_errors"] = dg.CurrentErrors
		m["current_errors_json"] = dg.CurrentErrorsJson
		m["latching_faults"] = dg.LatchingFaults
		m["latching_faults_json"] = dg.LatchingFaultsJson
		m["cells_v"] = dg.CellsV
		m["hv_charge_state"] = dg.HvChargeState
		m["lv_charge_state"] = dg.LvChargeState
		if len(dg.CellsV) > 0 {
			avg, min, max := statsFromFloat32Slice(dg.CellsV)
			m["avg_cell_v_stat"] = avg
			m["max_cell_v"] = max
			m["min_cell_v"] = min
		}
	}

	if t := msg.Thermal; t != nil {
		m["cells_temp"] = t.CellsTemp
		m["ambient_temp"] = t.AmbientTemp
		m["inverter_temp"] = t.InverterTemp
		m["motor_temp"] = t.MotorTemp
		m["water_motor_temp"] = t.WaterMotorTemp
		m["water_inverter_temp"] = t.WaterInverterTemp
		m["water_rad_temp"] = t.WaterRadTemp
		m["rad_fan_set"] = t.RadFanSet
		m["rad_fan_rpm"] = t.RadFanRpm
		m["batt_fan_set"] = t.BattFanSet
		m["batt_fan_rpm"] = t.BattFanRpm
		m["flow_rate"] = t.FlowRate
		if len(t.CellsTemp) > 0 {
			avg, min, max := statsFromInt32Slice(t.CellsTemp)
			m["avg_cell_temp_stat"] = avg
			m["max_cell_temp"] = max
			m["min_cell_temp"] = min
		}
	}

	return m
}

func orionToMap(msg *sensor.OrionSensorData) map[string]interface{} {
	m := make(map[string]interface{})
	m["time"] = msg.Time
	m["packet_id"] = msg.PacketId

	if d := msg.Dynamics; d != nil {
		m["gps"] = d.Gps
		m["gps_imu"] = d.GpsImu
		if len(d.Gps) >= 2 {
			m["latitude"] = d.Gps[0]
			m["longitude"] = d.Gps[1]
		}
		m["accel_pedal_travel"] = d.AccelPedalTravel
		m["steer_col_angle"] = d.SteerColAngle
		m["bl_sprung_accel"] = d.BlSprungAccel
		m["bl_unsprung_accel"] = d.BlUnsprungAccel
		m["br_sprung_accel"] = d.BrSprungAccel
		m["br_unsprung_accel"] = d.BrUnsprungAccel
		m["fl_sprung_accel"] = d.FlSprungAccel
		m["fl_unsprung_accel"] = d.FlUnsprungAccel
		m["fr_sprung_accel"] = d.FrSprungAccel
		m["fr_unsprung_accel"] = d.FrUnsprungAccel
		m["bl_ride_height"] = d.BlRideHeight
		m["bl_strain_gauge_v"] = d.BlStrainGaugeV
		m["bl_sus_pot_v"] = d.BlSusPotV
		m["blw_speed"] = d.BlwSpeed
		m["br_ride_height"] = d.BrRideHeight
		m["br_strain_gauge_v"] = d.BrStrainGaugeV
		m["br_sus_pot_v"] = d.BrSusPotV
		m["brw_speed"] = d.BrwSpeed
		m["fl_ride_height"] = d.FlRideHeight
		m["fl_strain_gauge_v"] = d.FlStrainGaugeV
		m["fl_sus_pot_v"] = d.FlSusPotV
		m["flw_speed"] = d.FlwSpeed
		m["fr_ride_height"] = d.FrRideHeight
		m["fr_strain_gauge_v"] = d.FrStrainGaugeV
		m["fr_sus_pot_v"] = d.FrSusPotV
		m["frw_speed"] = d.FrwSpeed
		m["ride_height"] = d.RideHeight
		m["wheel_speed"] = d.WheelSpeed
	}

	if c := msg.Controls; c != nil {
		m["motor_speed"] = c.MotorSpeed
		m["torque_feedback"] = c.TorqueFeedback
		m["apps1_travel"] = c.Apps1Travel
		m["apps1_v"] = c.Apps1V
		m["apps2_travel"] = c.Apps2Travel
		m["apps2_v"] = c.Apps2V
		m["bpps1_travel"] = c.Bpps1Travel
		m["bpps1_v"] = c.Bpps1V
		m["bpps2_travel"] = c.Bpps2Travel
		m["bpps2_v"] = c.Bpps2V
		m["brake_bias"] = c.BrakeBias
		m["brake_light_pct"] = c.BrakeLightPct
		m["brake_pressure_f"] = c.BrakePressureF
		m["brake_pressure_rall"] = c.BrakePressureRall
		m["brake_pressure_rbll"] = c.BrakePressureRbll
		m["bse1_v"] = c.Bse1V
		m["bse2_v"] = c.Bse2V
		m["bse3_v"] = c.Bse3V
		m["lights_current"] = c.LightsCurrent
		m["rpm_request"] = c.RpmRequest
		m["torque_command"] = c.TorqueCommand
		m["torque_limit"] = c.TorqueLimit
		m["torque_request"] = c.TorqueRequest
		m["commanded_torque"] = c.CommandedTorque
		m["motor_angle"] = c.MotorAngle
		m["direction"] = c.Direction
		m["enable"] = c.Enable
		m["torque_shudder"] = c.TorqueShudder
	}

	if p := msg.Pack; p != nil {
		m["bus_voltage"] = p.BusVoltage
		m["lv_boards_current"] = p.LvBoardsCurrent
		m["cells_v"] = p.CellsV
		m["dc_bus_v"] = p.DcBusV
		m["delta_resolver_angle"] = p.DeltaResolverAngle
		m["inverter_freq"] = p.InverterFreq
		m["neutral_output_v"] = p.NeutralOutputV
		m["time_since_on"] = p.TimeSinceOn
		m["vab_vq_v"] = p.VabVqV
		m["vbc_vd_v"] = p.VbcVdV
		m["cells_temps"] = p.CellsTemps
		m["dc_bus_current"] = p.DcBusCurrent
		m["hv_c"] = p.HvC
		m["hv_pack_v"] = p.HvPackV
		m["hv_soc"] = p.HvSoc
		m["lv_batt_c"] = p.LvBattC
		m["lv_batt_t"] = p.LvBattT
		m["lv_batt_v"] = p.LvBattV
		m["phase_a_current"] = p.PhaseACurrent
		m["phase_b_current"] = p.PhaseBCurrent
		m["phase_c_current"] = p.PhaseCCurrent

		if len(p.CellsV) > 0 {
			avg, min, max := statsFromFloat32Slice(p.CellsV)
			m["avg_cell_v_stat"] = avg
			m["max_cell_v"] = max
			m["min_cell_v"] = min
		}
		if len(p.CellsTemps) > 0 {
			avg, min, max := statsFromFloat32Slice(p.CellsTemps)
			m["avg_cell_temp_stat"] = avg
			m["max_cell_temp"] = max
			m["min_cell_temp"] = min
		}
	}

	if dh := msg.DiagnosticsHigh; dh != nil {
		m["prndl_state"] = dh.PrndlState
		m["shutdown_current"] = dh.ShutdownCurrent
		m["hvc_state_machine"] = dh.HvcStateMachine
		m["post_faults"] = dh.PostFaults
		m["run_faults"] = dh.RunFaults
		m["apps1_disconnect"] = dh.Apps1Disconnect
		m["apps1_out_range"] = dh.Apps1OutRange
		m["apps2_disconnect"] = dh.Apps2Disconnect
		m["apps2_out_range"] = dh.Apps2OutRange
		m["apps_implause"] = dh.AppsImplause
		m["apps_mismatch"] = dh.AppsMismatch
		m["batt_fans_fuse"] = dh.BattFansFuse
		m["batt_pump_fuse"] = dh.BattPumpFuse
		m["boards_fuse"] = dh.BoardsFuse
		m["bpps1_disconnect"] = dh.Bpps1Disconnect
		m["bpps1_out_range"] = dh.Bpps1OutRange
		m["bpps2_disconnect"] = dh.Bpps2Disconnect
		m["bpps2_out_range"] = dh.Bpps2OutRange
		m["bpps_mismatch"] = dh.BppsMismatch
		m["brake_light_fuse"] = dh.BrakeLightFuse
		m["bse1_disconnect"] = dh.Bse1Disconnect
		m["bse1_out_range"] = dh.Bse1OutRange
		m["bse2_disconnect"] = dh.Bse2Disconnect
		m["bse2_out_range"] = dh.Bse2OutRange
		m["ll_fuse"] = dh.LlFuse
		m["motor_pump_fuse"] = dh.MotorPumpFuse
		m["r2d_buzzer"] = dh.R2DBuzzer
		m["rtd_fuse"] = dh.RtdFuse
		m["shtdn_fuse"] = dh.ShtdnFuse
		m["shutdown_bspd_status"] = dh.ShutdownBspdStatus
		m["shutdown_emeter_status"] = dh.ShutdownEmeterStatus
		m["spare_fuse"] = dh.SpareFuse
		m["stomp_fault"] = dh.StompFault
		m["tssi_green_fuse"] = dh.TssiGreenFuse
		m["tssi_red_fuse"] = dh.TssiRedFuse
		m["neg_hv_contactor"] = dh.NegHvContactor
		m["pos_hv_contactor"] = dh.PosHvContactor
		m["precharge_contactor"] = dh.PrechargeContactor
	}

	if dl := msg.DiagnosticsLow; dl != nil {
		m["precharge_r_temp"] = dl.PrechargeRTemp
		m["bmb_comm_error"] = dl.BmbCommError
		m["imd_gnd_isolation_error"] = dl.ImdGndIsolationError
		m["r2d_authorized"] = dl.R2DAuthorized
		m["r2d_status"] = dl.R2DStatus
		m["shutdown_leg1"] = dl.ShutdownLeg1
		m["shutdown_leg2"] = dl.ShutdownLeg2
		m["shutdown_leg3"] = dl.ShutdownLeg3
		m["shutdown_leg4"] = dl.ShutdownLeg4
	}

	if t := msg.Thermal; t != nil {
		m["batt_cooling_current"] = t.BattCoolingCurrent
		m["motor_cooling_current"] = t.MotorCoolingCurrent
		m["motor_temp"] = t.MotorTemp
		m["ambient_temp"] = t.AmbientTemp
		m["batt_loop_batt_temp"] = t.BattLoopBattTemp
		m["batt_loop_rad_fan_speed"] = t.BattLoopRadFanSpeed
		m["batt_loop_rad_temp"] = t.BattLoopRadTemp
		m["battery_fan_rpm"] = t.BatteryFanRpm
		m["bus_bar_temp1"] = t.BusBarTemp1
		m["bus_bar_temp2"] = t.BusBarTemp2
		m["bus_bar_temp3"] = t.BusBarTemp3
		m["cell_bottom_temp"] = t.CellBottomTemp
		m["cell_top_temp"] = t.CellTopTemp
		m["coolant_flow_lpm"] = t.CoolantFlowLpm
		m["coolant_temp"] = t.CoolantTemp
		m["discharge_r_temp"] = t.DischargeRTemp
		m["fan_rpm"] = t.FanRpm
		m["gate_driver_temp"] = t.GateDriverTemp
		m["inverter_hotspot_temp"] = t.InverterHotspotTemp
		m["inverter_temp"] = t.InverterTemp
		m["module_a_temp"] = t.ModuleATemp
		m["module_b_temp"] = t.ModuleBTemp
		m["module_c_temp"] = t.ModuleCTemp
		m["motor_loop_inverter_temp"] = t.MotorLoopInverterTemp
		m["motor_loop_motor_temp"] = t.MotorLoopMotorTemp
		m["motor_loop_rad_temp"] = t.MotorLoopRadTemp
	}

	if bs := msg.BoardStatus; bs != nil {
		m["csm_last_seen_s"] = bs.CsmLastSeenS
		m["dui_last_seen_s"] = bs.DuiLastSeenS
		m["hvc_last_seen_s"] = bs.HvcLastSeenS
		m["inverter_last_seen_s"] = bs.InverterLastSeenS
		m["pdu_last_seen_s"] = bs.PduLastSeenS
		m["tsm_last_seen_s"] = bs.TsmLastSeenS
		m["usm_last_seen_s"] = bs.UsmLastSeenS
		m["vcu_last_seen_s"] = bs.VcuLastSeenS
	}

	return m
}

func main() {
	log.Println("Starting Kafka Bridge Service...")

	// Cars we support
	cars := []string{"angelique", "orion", "nightwatch"}

	// Create admin and ensure topics exist
	admin, err := newKafkaAdmin()
	if err != nil {
		log.Fatalf("Failed to create Kafka admin: %v", err)
	}
	defer admin.Close()

	for _, car := range cars {
		topic := fmt.Sprintf("grafana_data_%s", car)
		if err := ensureTopicExists(admin, topic); err != nil {
			log.Fatalf("Failed to ensure %s topic exists: %v", topic, err)
		}
	}

	if err := ensureTopicExists(admin, "sensor_data"); err != nil {
		log.Fatalf("Failed to ensure sensor_data topic exists: %v", err)
	}

	// List all topics for verification
	allTopics, err := admin.ListTopics()
	if err != nil {
		log.Printf("Warning: Could not list all topics: %v", err)
	} else {
		log.Printf("Available Kafka topics (%d):", len(allTopics))
		for topicName := range allTopics {
			log.Printf("  - %s", topicName)
		}
	}

	// Give Kafka a moment to propagate topic metadata
	log.Println("Waiting for topic metadata to propagate...")
	time.Sleep(3 * time.Second)

	producer, err := newKafkaProducer()
	if err != nil {
		log.Fatalf("Failed to create Kafka producer: %v", err)
	}
	defer producer.Close()

	// Seed topics to ensure they are viewable in grafana
	for _, car := range cars {
		seedTopic(producer, fmt.Sprintf("grafana_data_%s", car), car)
	}

	// Start worker goroutines
	go rawDataWorker(producer)
	go grafanaDataWorker(producer)

	// Start gRPC server
	listenAddr := os.Getenv("BRIDGE_ADDR")
	if listenAddr == "" {
		listenAddr = ":50051"
	}

	lis, err := net.Listen("tcp", listenAddr)
	if err != nil {
		log.Fatalf("Failed to listen: %v", err)
	}

	grpcServer := grpc.NewServer()
	pb.RegisterBridgeServiceServer(grpcServer, &bridgeServer{})

	// Graceful shutdown
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	go func() {
		<-sigChan
		log.Println("Shutting down...")
		grpcServer.GracefulStop()
		close(rawDataChan)
		close(grafanaDataChan)
	}()

	log.Printf("gRPC server listening on %s", listenAddr)
	if err := grpcServer.Serve(lis); err != nil {
		log.Fatalf("Failed to serve: %v", err)
	}
}

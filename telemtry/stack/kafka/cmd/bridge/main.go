package main

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"net"
	"os"
	"os/signal"
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

// Init method for kafka grafana datasource 
func seedTopic(producer sarama.SyncProducer, topic string) {
	// Send a minimal valid JSON payload that matches the structure Grafana expects
	seedData := map[string]interface{}{
		"time":       time.Now().UnixMilli(), // Use milliseconds for better Grafana compatibility
		"packet_id":  0,
		"car_type":   "init",
	}
	jsonData, err := json.Marshal(seedData)
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

// grafanaDataWorker deserializes protobuf and publishes JSON to grafana_data topic
func grafanaDataWorker(producer sarama.SyncProducer) {
	for msg := range grafanaDataChan {
		jsonData, err := deserializeToJSON(msg.Payload, msg.CarType)
		if err != nil {
			log.Printf("Error deserializing protobuf: %v", err)
			continue
		}

		kafkaMsg := &sarama.ProducerMessage{
			Topic: "grafana_data",
			Value: sarama.ByteEncoder(jsonData),
			Headers: []sarama.RecordHeader{
				{Key: []byte("car_type"), Value: []byte(msg.CarType)},
			},
		}

		_, _, err = producer.SendMessage(kafkaMsg)
		if err != nil {
			log.Printf("Error sending to grafana_data: %v", err)
		}
	}
}

func deserializeToJSON(payload []byte, carType string) ([]byte, error) {
	var data map[string]interface{}

	if carType == "Angelique" {
		msg := &sensor.AngeliqueSensorData{}
		if err := proto.Unmarshal(payload, msg); err != nil {
			return nil, err
		}
		data = angeliqueToMap(msg)
	} else {
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

	if msg.Dynamics != nil {
		m["steer_col_angle"] = msg.Dynamics.SteerColAngle
		m["fl_steer_angle"] = msg.Dynamics.FlSteerAngle
		m["fr_steer_angle"] = msg.Dynamics.FrSteerAngle
		m["flw_speed"] = msg.Dynamics.FlwSpeed
		m["frw_speed"] = msg.Dynamics.FrwSpeed
		m["blw_speed"] = msg.Dynamics.BlwSpeed
		m["brw_speed"] = msg.Dynamics.BrwSpeed
		m["fl_ride_height"] = msg.Dynamics.FlRideHeight
		m["fr_ride_height"] = msg.Dynamics.FrRideHeight
		m["bl_ride_height"] = msg.Dynamics.BlRideHeight
		m["br_ride_height"] = msg.Dynamics.BrRideHeight
		m["dash_speed"] = msg.Dynamics.DashSpeed
		m["f_gps_velocity"] = msg.Dynamics.FGpsVelocity
		m["b_gps_velocity"] = msg.Dynamics.BGpsVelocity
		if len(msg.Dynamics.FGps) >= 2 {
			m["latitude"] = msg.Dynamics.FGps[0]
			m["longitude"] = msg.Dynamics.FGps[1]
		}
	}

	if msg.Controls != nil {
		m["apps1_v"] = msg.Controls.Apps1V
		m["apps2_v"] = msg.Controls.Apps2V
		m["accel_pedal_t"] = msg.Controls.AccelPedalT
		m["brake_pedal_t"] = msg.Controls.BrakePedalT
		m["brake_pressure_f"] = msg.Controls.BrakePressureF
		m["brake_bias"] = msg.Controls.BrakeBias
	}

	if msg.Pack != nil {
		m["hv_pack_v"] = msg.Pack.HvPackV
		m["hv_tractive_v"] = msg.Pack.HvTractiveV
		m["hv_c"] = msg.Pack.HvC
		m["lv_v"] = msg.Pack.LvV
		m["lv_c"] = msg.Pack.LvC
		m["contactor_state"] = msg.Pack.ContactorState
		m["avg_cell_v"] = msg.Pack.AvgCellV
		m["avg_cell_temp"] = msg.Pack.AvgCellTemp
	}

	if msg.Thermal != nil {
		m["motor_temp"] = msg.Thermal.MotorTemp
		m["inverter_temp"] = msg.Thermal.InverterTemp
		m["ambient_temp"] = msg.Thermal.AmbientTemp
		m["batt_over_temp"] = msg.Thermal.BattOverTemp
	}

	if msg.DiagnosticsLow != nil {
		m["cells_min_v"] = msg.DiagnosticsLow.CellMinV
		m["cells_max_v"] = msg.DiagnosticsLow.CellMaxV
		if len(msg.DiagnosticsLow.CellsTemps) > 0 {
			avg, min, max := statsFromFloat32Slice(msg.DiagnosticsLow.CellsTemps)
			m["avg_cell_temp_stat"] = avg
			m["max_cell_temp"] = max
			m["min_cell_temp"] = min
		}
	}

	return m
}

func angeliqueToMap(msg *sensor.AngeliqueSensorData) map[string]interface{} {
	m := make(map[string]interface{})
	m["time"] = msg.Time
	m["packet_id"] = msg.PacketId

	if msg.Dynamics != nil {
		m["torque_request"] = msg.Dynamics.TorqueRequest
		m["gps_velocity"] = msg.Dynamics.GpsVelocity
		m["gps_heading"] = msg.Dynamics.GpsHeading
		m["flw_speed"] = msg.Dynamics.FlwSpeed
		m["frw_speed"] = msg.Dynamics.FrwSpeed
		m["front_speed"] = (msg.Dynamics.FlwSpeed + msg.Dynamics.FrwSpeed) / 2
		m["blw_speed"] = msg.Dynamics.BlwSpeed
		m["brw_speed"] = msg.Dynamics.BrwSpeed
		m["rear_speed"] = (msg.Dynamics.BlwSpeed + msg.Dynamics.BrwSpeed) / 2
		m["inverter_v"] = msg.Dynamics.InverterV
		m["inverter_c"] = msg.Dynamics.InverterC
		electricalPower := msg.Dynamics.InverterV * float32(msg.Dynamics.InverterC)
		mechanicalPower := float32(msg.Dynamics.InverterRpm) * float32(msg.Dynamics.InverterTorque) / 9.5490
		m["inverter_electrical_power"] = electricalPower
		m["inverter_mechanical_power"] = mechanicalPower
		if electricalPower != 0 {
			m["efficiency"] = mechanicalPower / electricalPower
		} else {
			m["efficiency"] = float32(0)
		}
		m["inverter_rpm"] = msg.Dynamics.InverterRpm
		m["inverter_torque"] = msg.Dynamics.InverterTorque
		if len(msg.Dynamics.Gps) >= 2 {
			m["latitude"] = msg.Dynamics.Gps[0]
			m["longitude"] = msg.Dynamics.Gps[1]
		}
	}

	if msg.Controls != nil {
		m["apps1_v"] = msg.Controls.Apps1V
		m["apps2_v"] = msg.Controls.Apps2V
		m["bse1_v"] = msg.Controls.Bse1V
		m["bse2_v"] = msg.Controls.Bse2V
		m["steer_v"] = msg.Controls.SteerV
	}

	if msg.Pack != nil {
		m["hv_pack_v"] = msg.Pack.HvPackV
		m["hv_tractive_v"] = msg.Pack.HvTractiveV
		m["hv_c"] = msg.Pack.HvC
		m["lv_v"] = msg.Pack.LvV
		m["lv_c"] = msg.Pack.LvC
		m["contactor_state"] = msg.Pack.ContactorState
		m["avg_cell_v"] = msg.Pack.AvgCellV
		m["avg_cell_temp"] = msg.Pack.AvgCellTemp
		m["hv_power"] = msg.Pack.HvPackV * float32(msg.Pack.HvC)
		m["lv_power"] = msg.Pack.LvV * float32(msg.Pack.LvC)
	}

	if msg.Diagnostics != nil {
		m["hv_charge_state"] = msg.Diagnostics.HvChargeState
		m["lv_charge_state"] = msg.Diagnostics.LvChargeState
		if len(msg.Diagnostics.CellsV) > 0 {
			avg, min, max := statsFromFloat32Slice(msg.Diagnostics.CellsV)
			m["avg_cell_v_stat"] = avg
			m["max_cell_v"] = max
			m["min_cell_v"] = min
		}
	}

	if msg.Thermal != nil {
		m["ambient_temp"] = msg.Thermal.AmbientTemp
		m["inverter_temp"] = msg.Thermal.InverterTemp
		m["motor_temp"] = msg.Thermal.MotorTemp
		m["flow_rate"] = msg.Thermal.FlowRate

		// Compute average, min and max of cell temperatures (repeated int32)
		if len(msg.Thermal.CellsTemp) > 0 {
			avg, min, max := statsFromInt32Slice(msg.Thermal.CellsTemp)
			m["avg_cell_temp_stat"] = avg
			m["max_cell_temp"] = max
			m["min_cell_temp"] = min
		}
	}

	return m
}

func main() {
	log.Println("Starting Kafka Bridge Service...")

	// Create admin and ensure grafana_data topic exists so Grafana always sees it
	admin, err := newKafkaAdmin()
	if err != nil {
		log.Fatalf("Failed to create Kafka admin: %v", err)
	}
	defer admin.Close()

	if err := ensureTopicExists(admin, "grafana_data"); err != nil {
		log.Fatalf("Failed to ensure grafana_data topic exists: %v", err)
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

	// Seed grafana_data topic to ensure it exists and viewable in grafana
	seedTopic(producer, "grafana_data")

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

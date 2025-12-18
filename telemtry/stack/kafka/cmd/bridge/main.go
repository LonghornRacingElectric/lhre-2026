package main

import (
	"context"
	"encoding/json"
	"log"
	"net"
	"os"
	"os/signal"
	"syscall"

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
	rawDataChan     = make(chan SensorMessage, 1000)
	grafanaDataChan = make(chan SensorMessage, 1000)
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
		m["blw_speed"] = msg.Dynamics.BlwSpeed
		m["brw_speed"] = msg.Dynamics.BrwSpeed
		m["inverter_v"] = msg.Dynamics.InverterV
		m["inverter_c"] = msg.Dynamics.InverterC
		m["inverter_rpm"] = msg.Dynamics.InverterRpm
		m["inverter_torque"] = msg.Dynamics.InverterTorque
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
	}

	if msg.Diagnostics != nil {
		m["hv_charge_state"] = msg.Diagnostics.HvChargeState
		m["lv_charge_state"] = msg.Diagnostics.LvChargeState
	}

	if msg.Thermal != nil {
		m["ambient_temp"] = msg.Thermal.AmbientTemp
		m["inverter_temp"] = msg.Thermal.InverterTemp
		m["motor_temp"] = msg.Thermal.MotorTemp
		m["flow_rate"] = msg.Thermal.FlowRate
	}

	return m
}

func main() {
	log.Println("Starting Kafka Bridge Service...")

	producer, err := newKafkaProducer()
	if err != nil {
		log.Fatalf("Failed to create Kafka producer: %v", err)
	}
	defer producer.Close()

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

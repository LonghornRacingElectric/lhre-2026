import CarVisualization from "@/components/CarVisualization";

export default function TestPage() {
    return (
        <div>
            <CarVisualization
                wheel_speed={2}
                fl_wheel_angle={0.3}
                fr_wheel_angle={0.3}
                fl_spring_displace={0.5}
                fr_spring_displace={0.2}
                bl_spring_displace={0.1}
                br_spring_displace={0.3}
            />
        </div>
    )
}
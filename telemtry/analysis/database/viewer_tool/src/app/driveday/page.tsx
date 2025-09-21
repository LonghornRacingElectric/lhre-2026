
'use client';

import { useRouter } from 'next/navigation';
import { Button } from "@/components/ui/button";
import {
  Card,
  CardContent,
  CardDescription,
  CardFooter,
  CardHeader,
  CardTitle,
} from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";

export default function DrivedayPage() {
  const router = useRouter();

  const handleCreate = () => {
    router.push('/event/new');
  };

  return (
    <div className="p-8 flex justify-center items-center">
      <Card className="w-[350px]">
        <CardHeader>
          <CardTitle>Create New Drive Day</CardTitle>
          <CardDescription>Enter the details for the new drive day.</CardDescription>
        </CardHeader>
        <CardContent>
          <form>
            <div className="grid w-full items-center gap-4">
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="power_limit">Power Limit</Label>
                <Input id="power_limit" placeholder="Enter power limit" />
              </div>
              <div className="flex flex-col space-y-1.5">
                <Label htmlFor="conditions">Driving Conditions</Label>
                <Input id="conditions" placeholder="Enter driving conditions" />
              </div>
            </div>
          </form>
        </CardContent>
        <CardFooter className="flex justify-between">
          <Button variant="outline">Cancel</Button>
          <Button onClick={handleCreate}>Create</Button>
        </CardFooter>
      </Card>
    </div>
  );
}
